#include "networkaccess.h"
#include "constants.h"
#include <QtGui>

namespace The {
    NetworkAccess* http();
}

NetworkReply::NetworkReply(QNetworkReply *networkReply) : QObject(networkReply) {
    this->networkReply = networkReply;
    followRedirects = true;

    timer = new QTimer(this);
    timer->setInterval(60000);
    timer->setSingleShot(true);
    connect(timer, SIGNAL(timeout()), SLOT(abort()));
    timer->start();
}

void NetworkReply::finished() {
    timer->stop();

    if (networkReply->error() != QNetworkReply::NoError) {
        qDebug() << networkReply->url() << "finished with error" << networkReply->error();
        networkReply->deleteLater();
        return;
    }

    if (followRedirects) {
        QUrl redirection = networkReply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (redirection.isValid()) {

            // qDebug() << "Redirect!"; // << redirection;

            QNetworkReply *redirectReply = The::http()->simpleGet(redirection, networkReply->operation());

            setParent(redirectReply);
            networkReply->deleteLater();
            networkReply = redirectReply;

            // when the request is finished we'll invoke the target method
            connect(networkReply, SIGNAL(finished()), this, SLOT(finished()), Qt::QueuedConnection);

            return;
        }
    }

    emit data(networkReply->readAll());
    emit finished(networkReply);

#ifndef QT_NO_DEBUG_OUTPUT
    if (!networkReply->attribute(QNetworkRequest::SourceIsFromCacheAttribute).toBool()) {
        qDebug() << networkReply->url().toEncoded();
    }
#endif

    // bye bye my reply
    // this will also delete this NetworkReply as the QNetworkReply is its parent
    networkReply->deleteLater();
}

void NetworkReply::requestError(QNetworkReply::NetworkError /*code*/) {
    timer->stop();
    emit error(networkReply);
}

void NetworkReply::abort() {
    qDebug() << "HTTP timeout" << networkReply->url();
    networkReply->abort();
    emit error(networkReply);
}

/* --- NetworkAccess --- */

NetworkAccess::NetworkAccess(QObject* parent) : QObject( parent ) {}

QNetworkReply* NetworkAccess::simpleGet(QUrl url, int operation) {

    QNetworkAccessManager *manager = The::networkAccessManager();

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", USER_AGENT.toUtf8());
    request.setRawHeader("Connection", "Keep-Alive");

    QNetworkReply *networkReply;
    switch (operation) {

    case QNetworkAccessManager::GetOperation:
        // qDebug() << "GET" << url.toEncoded();
        networkReply = manager->get(request);
        break;

    case QNetworkAccessManager::HeadOperation:
        // qDebug() << "HEAD" << url.toEncoded();
        networkReply = manager->head(request);
        break;

    default:
        qDebug() << "Unknown operation:" << operation;
        return 0;

    }

    // error handling
    connect(networkReply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(error(QNetworkReply::NetworkError)), Qt::QueuedConnection);

    return networkReply;

}

NetworkReply* NetworkAccess::get(const QUrl url) {

    QNetworkReply *networkReply = simpleGet(url);
    NetworkReply *reply = new NetworkReply(networkReply);

    // error signal
    connect(networkReply, SIGNAL(error(QNetworkReply::NetworkError)),
            reply, SLOT(requestError(QNetworkReply::NetworkError)), Qt::QueuedConnection);

    // when the request is finished we'll invoke the target method
    connect(networkReply, SIGNAL(finished()), reply, SLOT(finished()), Qt::QueuedConnection);

    return reply;

}

NetworkReply* NetworkAccess::head(const QUrl url) {

    QNetworkReply *networkReply = simpleGet(url, QNetworkAccessManager::HeadOperation);
    NetworkReply *reply = new NetworkReply(networkReply);
    reply->followRedirects = false;

    // error signal
    connect(networkReply, SIGNAL(error(QNetworkReply::NetworkError)),
            reply, SLOT(requestError(QNetworkReply::NetworkError)), Qt::QueuedConnection);

    // when the request is finished we'll invoke the target method
    connect(networkReply, SIGNAL(finished()), reply, SLOT(finished()), Qt::QueuedConnection);

    return reply;

}

void NetworkAccess::error(QNetworkReply::NetworkError code) {
    // get the QNetworkReply that sent the signal
    QNetworkReply *networkReply = static_cast<QNetworkReply *>(sender());
    if (!networkReply) {
        qDebug() << "Cannot get sender";
        return;
    }

    qDebug() << networkReply->errorString() << code;

    networkReply->deleteLater();
}
