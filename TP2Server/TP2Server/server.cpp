#include <QtNetwork>

#include <stdlib.h>
#include <iostream>
#include <stdio.h>

#include "server.h"

Server::Server(int numResources[], QWidget *parent)
:   parentWidget(parent)
{
    tcpServer = new QTcpServer(parentWidget);
    // For this QTcpServer whenever a new conection is detected
    // we call the function proccesRequest()
    connect(tcpServer, SIGNAL(newConnection()), this, SLOT(processRequest()));
    logStream.setString(&logString);

    //Initialization of data structures
    for (int i = 0 ; i < NUMBER_OF_RESOURCES ; i++) {
        available[i] = numResources[i];
        for (int j = 0 ; j < NUMBER_OF_CUSTOMERS ; j++) {
            maximum[j][i] = -1;
            allocation[j][i] = 0;
            need[j][i] = 0;
        }
    }
}

/// This function is called whenever the system
/// receives a connection. The function "nextPendingConnection()"
/// takes one request from a internal list of pending connections
/// and returns it as a object QTcpSoket, to read, or write on it
void Server::processRequest()
{
    QTcpSocket *clientConnection = tcpServer->nextPendingConnection();
    logStream << "-Processing request from ip: " << clientConnection->peerAddress().toString();
    printStreamToLog();

    // Reading the maximum requested resources
    clientConnection->waitForReadyRead();
    char maximumMsg[20] = "empty";
    qDebug() << clientConnection->read(maximumMsg, 20);
    QStringList maximumMsgList = QString(maximumMsg).split(" ");
    int user = maximumMsgList[0].toInt();
    maximum[user][0] = maximumMsgList[2].toInt();
    maximum[user][1] = maximumMsgList[3].toInt();
    maximum[user][2] = maximumMsgList[4].toInt();

    // Writing needs in database
    need[user][0] = maximum[user][0] - allocation[user][0];
    need[user][1] = maximum[user][1] - allocation[user][1];
    need[user][2] = maximum[user][2] - allocation[user][2];

    // Reading the request
    clientConnection->waitForReadyRead();
    char requestMsg[20] = " ";
    qDebug() << clientConnection->read(requestMsg, 20);
    logStream << QString(requestMsg);
    printStreamToLog();
    QStringList requestMsgList = QString(requestMsg).split(" ");
    //int user = requestMsgList[0].toInt();
    int request[3];
    request[0] = requestMsgList[2].toInt();
    request[1] = requestMsgList[3].toInt();
    request[2] = requestMsgList[4].toInt();

    // Processing the request
    QString responseMsg;
    if (validateRequest(user, request) == -1)
        responseMsg = QString::number(-1);
    if (validateRequest(user, request) == 1)
        responseMsg = QString::number(1);
    if (validateRequest(user, request) == 0) {
        responseMsg = QString::number(0);
        addInstancesOfResources(request);
    }

    // Writing response
    clientConnection->write(responseMsg.toStdString().c_str());
    clientConnection->waitForBytesWritten();

    //Close conection and free the memory once the connection is lost
    connect(clientConnection, SIGNAL(disconnected()), clientConnection, SLOT(deleteLater()));
    clientConnection->disconnectFromHost();
}

/// This funtion should update the values of the
/// data structures when a request is granted
void Server::addInstancesOfResources(int numberOfInstances[NUMBER_OF_RESOURCES])
{
    /// This is to update the values of the resource meters
    /// in the GUI, (positive values add to the respective
    /// meter bar and negative values reduce it)
    for (int i = 0 ; i < NUMBER_OF_RESOURCES ; i++)
        emit updateAddResourceInGUI(i, numberOfInstances[i]);
}

/// This function does the Banker's algorithm with the current request
/// and simply returns whether the request is valid, invalid or if it should wait
int Server::validateRequest(int user, int request[3]) {
    if (-request[0] > need[user][0] || -request[1] > need[user][1] || -request[2] > need[user][2])
        return -1;
    if (-request[0] > available[0] || -request[1] > available[1] || -request[2] > available[2])
        return 1;

    for (int i = 0 ; i < NUMBER_OF_RESOURCES ; i++) {
         available[i] += request[i];
         allocation[user][i] -= request[i];
         need[user][i] += request[i];
    }

    if (false) {
        for (int i = 0 ; i < NUMBER_OF_RESOURCES ; i++) {
            available[i] -= request[i];
            allocation[user][i] += request[i];
            need[user][i] -= request[i];
        }
        return 1;
    }

    return 0;
}

bool safeState() {
    return true;
}

/// This is a convinient function to easily print to the log window in the GUI
/// Call it after adding some output to "logStream"
void Server::printStreamToLog(){
    emit sendMessageToLog(logString);
    logStream.flush();
    logString.clear();
}

/// This function makes the server to listen to the
/// address and port specified by the GUI and prints a message to the GUI log
bool Server::startServer(const QString & address, quint16 port)
{
    if (tcpServer->listen(QHostAddress(address),port))
    {
        logStream << tr("The server started on\n\nIP: %1\nport: %2\n\n").arg(address).arg(tcpServer->serverPort());
        logStream << "with " << NUMBER_OF_RESOURCES << " types of resources:\n";
        for (int i = 0 ; i < NUMBER_OF_RESOURCES ; i++)
            logStream << available[i] << " ";
        logStream << "\n-Start the client application now.";
        printStreamToLog();
        return true;
    }

    logStream << tr("-Unable to start the server: %1.").arg(tcpServer->errorString());
    printStreamToLog();
    return false;
}

/// This function stops the server and prints a message to the GUI log
void Server::stopServer()
{
    tcpServer->close();
    logStream << "-Server has stopped\n";
    logStream << "-----------------------";
    printStreamToLog();
}

/// This funtion returns true if server is running, false otherwise
bool Server::isRunning(){
    return tcpServer->isListening();
}

/// This funtion returns the port number
int Server::getPort(){
    return tcpServer->serverPort();
}