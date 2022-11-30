// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SERVER_WEB_SOCKET_H_
#define NET_SERVER_WEB_SOCKET_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/websockets/websocket_frame.h"

namespace net {

class HttpConnection;
class HttpServer;
class HttpServerRequestInfo;
class WebSocketEncoder;

class WebSocket final {
 public:
  enum ParseResult {
    // Final frame of a text message or compressed frame.
    FRAME_OK_FINAL,
    // Other frame of a text message.
    FRAME_OK_MIDDLE,
    FRAME_PING,
    FRAME_PONG,
    FRAME_INCOMPLETE,
    FRAME_CLOSE,
    FRAME_ERROR
  };

  WebSocket(HttpServer* server, HttpConnection* connection);

  void Accept(const HttpServerRequestInfo& request,
              const NetworkTrafficAnnotationTag traffic_annotation);
  ParseResult Read(std::string* message);
  void Send(base::StringPiece message,
            WebSocketFrameHeader::OpCodeEnum op_code,
            const NetworkTrafficAnnotationTag traffic_annotation);

  WebSocket(const WebSocket&) = delete;
  WebSocket& operator=(const WebSocket&) = delete;

  ~WebSocket();

 private:
  void Fail();
  void SendErrorResponse(const std::string& message,
                         const NetworkTrafficAnnotationTag traffic_annotation);

  const raw_ptr<HttpServer> server_;
  const raw_ptr<HttpConnection> connection_;
  std::unique_ptr<WebSocketEncoder> encoder_;
  bool closed_ = false;
  std::unique_ptr<NetworkTrafficAnnotationTag> traffic_annotation_ = nullptr;
};

}  // namespace net

#endif  // NET_SERVER_WEB_SOCKET_H_
