// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/websocket_close_handler.h"

#include "net/test/embedded_test_server/create_websocket_handler.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace net::test_server {

WebSocketCloseHandler::WebSocketCloseHandler(
    scoped_refptr<WebSocketConnection> connection)
    : WebSocketHandler(std::move(connection)) {}

WebSocketCloseHandler::~WebSocketCloseHandler() = default;

void WebSocketCloseHandler::OnTextMessage(std::string_view message) {
  CHECK(connection());

  // If the message is "Goodbye", initiate a closing handshake.
  if (message == "Goodbye") {
    connection()->StartClosingHandshake(1000, "Goodbye");
  }
}

EmbeddedTestServer::HandleUpgradeRequestCallback
WebSocketCloseHandler::CreateHandler() {
  return CreateWebSocketHandler(
      "/close",
      base::BindRepeating([](scoped_refptr<WebSocketConnection> connection)
                              -> std::unique_ptr<WebSocketHandler> {
        return std::make_unique<WebSocketCloseHandler>(connection);
      }));
}

}  // namespace net::test_server
