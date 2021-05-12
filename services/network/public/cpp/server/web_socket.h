// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SERVER_WEB_SOCKET_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SERVER_WEB_SOCKET_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace network {

namespace server {

class HttpConnection;
class HttpServer;
class HttpServerRequestInfo;
class WebSocketEncoder;

class WebSocket final {
 public:
  enum ParseResult { FRAME_OK, FRAME_INCOMPLETE, FRAME_CLOSE, FRAME_ERROR };

  WebSocket(HttpServer* server, HttpConnection* connection);

  void Accept(const HttpServerRequestInfo& request,
              const net::NetworkTrafficAnnotationTag traffic_annotation);
  ParseResult Read(std::string* message);
  void Send(base::StringPiece message,
            const net::NetworkTrafficAnnotationTag traffic_annotation);
  ~WebSocket();

 private:
  void Fail();
  void SendErrorResponse(
      const std::string& message,
      const net::NetworkTrafficAnnotationTag traffic_annotation);

  HttpServer* const server_;
  HttpConnection* const connection_;
  std::unique_ptr<WebSocketEncoder> encoder_;
  bool closed_;

  DISALLOW_COPY_AND_ASSIGN(WebSocket);
};

}  // namespace server

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SERVER_WEB_SOCKET_H_
