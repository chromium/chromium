// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/websocket_echo_handler.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "net/test/embedded_test_server/create_websocket_handler.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/websocket_connection.h"
#include "net/test/embedded_test_server/websocket_handler.h"

namespace net::test_server {

WebSocketEchoHandler::WebSocketEchoHandler(
    scoped_refptr<WebSocketConnection> connection)
    : WebSocketHandler(std::move(connection)) {}

void WebSocketEchoHandler::OnHandshake(const HttpRequest& request) {
  CHECK(connection());
  connection()->SetResponseHeader("X-Custom-Header", "WebSocketEcho");
}

void WebSocketEchoHandler::OnTextMessage(std::string_view message) {
  CHECK(connection());
  connection()->SendTextMessage(message);
}

void WebSocketEchoHandler::OnBinaryMessage(base::span<const uint8_t> message) {
  CHECK(connection());
  connection()->SendBinaryMessage(message);
}

void WebSocketEchoHandler::OnClosingHandshake(std::optional<uint16_t> code,
                                              std::string_view message) {
  DVLOG(3) << "Closing handshake received with code: "
           << (code.has_value() ? base::NumberToString(code.value()) : "none")
           << ", message: " << message;

  connection()->RespondToCloseFrame(code, message);
}

EmbeddedTestServer::HandleUpgradeRequestCallback
WebSocketEchoHandler::CreateHandler() {
  return CreateWebSocketHandler(
      "/echo-with-no-extension",
      base::BindRepeating([](scoped_refptr<WebSocketConnection> connection)
                              -> std::unique_ptr<WebSocketHandler> {
        return std::make_unique<WebSocketEchoHandler>(connection);
      }));
}

}  // namespace net::test_server
