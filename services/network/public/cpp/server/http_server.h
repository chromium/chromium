// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SERVER_HTTP_SERVER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SERVER_HTTP_SERVER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace net {

class IPEndPoint;
}

namespace network {

namespace server {

class HttpConnection;
class HttpServerRequestInfo;
class HttpServerResponseInfo;

class COMPONENT_EXPORT(NETWORK_CPP) HttpServer {
 public:
  // Delegate to handle http/websocket events. Beware that it is not safe to
  // destroy the HttpServer in any of these callbacks.
  class Delegate {
   public:
    virtual ~Delegate() {}

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
  HttpServer(mojo::PendingRemote<mojom::TCPServerSocket> server_socket,
             HttpServer::Delegate* delegate);
  ~HttpServer();

  void AcceptWebSocket(int connection_id,
                       const HttpServerRequestInfo& request,
                       net::NetworkTrafficAnnotationTag traffic_annotation);
  void SendOverWebSocket(int connection_id,
                         base::StringPiece data,
                         net::NetworkTrafficAnnotationTag traffic_annotation);
  // Sends the provided data directly to the given connection. No validation is
  // performed that data constitutes a valid HTTP response. A valid HTTP
  // response may be split across multiple calls to SendRaw.
  void SendRaw(int connection_id,
               const std::string& data,
               net::NetworkTrafficAnnotationTag traffic_annotation);
  // TODO(byungchul): Consider replacing function name with SendResponseInfo
  void SendResponse(int connection_id,
                    const HttpServerResponseInfo& response,
                    net::NetworkTrafficAnnotationTag traffic_annotation);
  void Send(int connection_id,
            net::HttpStatusCode status_code,
            const std::string& data,
            const std::string& mime_type,
            net::NetworkTrafficAnnotationTag traffic_annotation);
  void Send200(int connection_id,
               const std::string& data,
               const std::string& mime_type,
               net::NetworkTrafficAnnotationTag traffic_annotation);
  void Send404(int connection_id,
               net::NetworkTrafficAnnotationTag traffic_annotation);
  void Send500(int connection_id,
               const std::string& message,
               net::NetworkTrafficAnnotationTag traffic_annotation);

  void Close(int connection_id);

  // These two functions set the read and write buffers of the HttpConnection
  // identified by |connection_id|.  These return |true| on success, and return
  // |false| if there is no object indexed by |connection_id|.
  bool SetReceiveBufferSize(int connection_id, int32_t size);
  bool SetSendBufferSize(int connection_id, int32_t size);

 private:
  friend class HttpServerTest;

  void DoAcceptLoop();
  void OnAcceptCompleted(
      int rv,
      const base::Optional<net::IPEndPoint>& remote_addr,
      mojo::PendingRemote<mojom::TCPConnectedSocket> connected_socket,
      mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
      mojo::ScopedDataPipeProducerHandle send_pipe_handle);

  void OnReadable(int connection_id,
                  MojoResult result,
                  const mojo::HandleSignalsState& state);
  void OnReadCompleted(int connection_id, MojoResult rv);
  void HandleReadResult(HttpConnection* connection, MojoResult rv);

  void OnWritable(int connection_id, MojoResult result);
  void OnWriteCompleted(int connection_id,
                        net::NetworkTrafficAnnotationTag traffic_annotation,
                        int rv);

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

  const mojo::Remote<mojom::TCPServerSocket> server_socket_;
  HttpServer::Delegate* const delegate_;

  int last_id_;
  std::map<int, std::unique_ptr<HttpConnection>> id_to_connection_;

  base::WeakPtrFactory<HttpServer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HttpServer);
};

}  // namespace server

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SERVER_HTTP_SERVER_H_
