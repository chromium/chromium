// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/websocket_check_origin_handler.h"

#include "base/logging.h"

namespace net::test_server {

WebSocketCheckOriginHandler::WebSocketCheckOriginHandler(
    scoped_refptr<WebSocketConnection> connection)
    : WebSocketHandler(std::move(connection)) {}

WebSocketCheckOriginHandler::~WebSocketCheckOriginHandler() = default;

void WebSocketCheckOriginHandler::OnHandshake(const HttpRequest& request) {
  // Retrieve and store the origin from the request headers.
  auto it = request.headers.find("Origin");

  CHECK(it != request.headers.end());
  origin_ = it->second;
  VLOG(3) << "Stored WebSocket origin: " << origin_;
}

void WebSocketCheckOriginHandler::OnHandshakeComplete() {
  CHECK(connection());
  VLOG(3) << "Sending stored origin after handshake completion: " << origin_;
  connection()->SendTextMessage(origin_);
  connection()->StartClosingHandshake(1000, "Goodbye");
}

}  // namespace net::test_server
