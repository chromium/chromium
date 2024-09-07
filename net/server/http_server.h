// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_HTTP_SERVER_H_
#define NET_SERVER_HTTP_SERVER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class HttpConnection;
class HttpServerRequestInfo;
class HttpServerResponseInfo;
class IPEndPoint;
class ServerSocket;
class StreamSocket;

class NET_EXPORT HttpServer {
 public:
  // Delegate to handle http/websocket events. Beware that it is not safe to
  // destroy the HttpServer in any of these callbacks.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnConnect(int connection_id) = 0;
    virtual void OnHttpRequest(int connection_id,
                               const HttpServerRequestInfo& info) = 0;
    virtual void OnWebSocketRequest(int connection_id,
                                    const HttpServerRequestInfo& info) = 0;
    virtual void OnWebSocketMessage(int connection_id, std::string data) = 0;
    virtual void OnClose(int connection_id) = 0;
  };

  // Instantiates a http server with |server_socket| which already started
  // listening, but not accepting.  This constructor schedules accepting
  // connections asynchronously in case when |delegate| is not ready to get
  // callbacks yet.
  HttpServer(std::unique_ptr<ServerSocket> server_socket,
             HttpServer::Delegate* delegate);

  HttpServer(const HttpServer&) = delete;
  HttpServer& operator=(const HttpServer&) = delete;

  ~HttpServer();

  void AcceptWebSocket(int connection_id,
                       const HttpServerRequestInfo& request,
                       NetworkTrafficAnnotationTag traffic_annotation);
  void SendOverWebSocket(int connection_id,
                         std::string_view data,
                         NetworkTrafficAnnotationTag traffic_annotation);
  // Sends the provided data directly to the given connection. No validation is
  // performed that data constitutes a valid HTTP response. A valid HTTP
  // response may be split across multiple calls to SendRaw.
  void SendRaw(int connection_id,
               const std::string& data,
               NetworkTrafficAnnotationTag traffic_annotation);
  // TODO(byungchul): Consider replacing function name with SendResponseInfo
  void SendResponse(int connection_id,
                    const HttpServerResponseInfo& response,
                    NetworkTrafficAnnotationTag traffic_annotation);
  void Send(int connection_id,
            HttpStatusCode status_code,
            const std::string& data,
            const std::string& mime_type,
            NetworkTrafficAnnotationTag traffic_annotation);
  void Send200(int connection_id,
               const std::string& data,
               const std::string& mime_type,
               NetworkTrafficAnnotationTag traffic_annotation);
  void Send404(int connection_id,
               NetworkTrafficAnnotationTag traffic_annotation);
  void Send500(int connection_id,
               const std::string& message,
               NetworkTrafficAnnotationTag traffic_annotation);

  void Close(int connection_id);

  void SetReceiveBufferSize(int connection_id, int32_t size);
  void SetSendBufferSize(int connection_id, int32_t size);

  // Copies the local address to |address|. Returns a network error code.
  int GetLocalAddress(IPEndPoint* address);

 private:
  friend class HttpServerTest;

  void DoAcceptLoop();
  void OnAcceptCompleted(int rv);
  int HandleAcceptResult(int rv);

  void DoReadLoop(HttpConnection* connection);
  void OnReadCompleted(int connection_id, int rv);
  int HandleReadResult(HttpConnection* connection, int rv);

  void DoWriteLoop(HttpConnection* connection,
                   NetworkTrafficAnnotationTag traffic_annotation);
  void OnWriteCompleted(int connection_id,
                        NetworkTrafficAnnotationTag traffic_annotation,
                        int rv);
  int HandleWriteResult(HttpConnection* connection, int rv);

  // Expects the raw data to be stored in recv_data_. If parsing is successful,
  // will remove the data parsed from recv_data_, leaving only the unused
  // recv data. If all data has been consumed successfully, but the headers are
  // not fully parsed, *pos will be set to zero. Returns false if an error is
  // encountered while parsing, true otherwise.
  bool ParseHeaders(const char* data,
                    size_t data_len,
                    HttpServerRequestInfo* info,
                    size_t* pos);

  HttpConnection* FindConnection(int connection_id);

  // Whether or not Close() has been called during delegate callback processing.
  bool HasClosedConnection(HttpConnection* connection);

  void DestroyClosedConnections();

  const std::unique_ptr<ServerSocket> server_socket_;
  std::unique_ptr<StreamSocket> accepted_socket_;
  const raw_ptr<HttpServer::Delegate> delegate_;

  int last_id_ = 0;
  std::map<int, std::unique_ptr<HttpConnection>> id_to_connection_;

  // Vector of connections whose destruction is pending. Connections may have
  // WebSockets with raw pointers to `this`, so should not out live this, but
  // also cannot safely be destroyed synchronously, so on connection close, add
  // a Connection here, and post a task to destroy them.
  std::vector<std::unique_ptr<HttpConnection>> closed_connections_;

  base::WeakPtrFactory<HttpServer> weak_ptr_factory_{this};
};

}  // namespace net

#endif  // NET_SERVER_HTTP_SERVER_H_
