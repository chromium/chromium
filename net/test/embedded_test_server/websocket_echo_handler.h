// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_ECHO_HANDLER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_ECHO_HANDLER_H_

#include <memory>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/websocket_connection.h"
#include "net/test/embedded_test_server/websocket_handler.h"

namespace net::test_server {

// WebSocketEchoHandler is a handler for WebSocket connections that echoes
// back any received text or binary messages to the sender.
class WebSocketEchoHandler : public WebSocketHandler {
 public:
  // Constructs the handler with a given WebSocket connection.
  explicit WebSocketEchoHandler(scoped_refptr<WebSocketConnection> connection);

  // Called during the WebSocket handshake; adds an "X-Custom-Header" with the
  // value "WebSocketEcho" to the response.
  void OnHandshake(const HttpRequest& request) override;

  // Echoes back any received text message.
  void OnTextMessage(std::string_view message) override;

  // Echoes back any received binary message.
  void OnBinaryMessage(base::span<const uint8_t> message) override;
};

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_ECHO_HANDLER_H_
