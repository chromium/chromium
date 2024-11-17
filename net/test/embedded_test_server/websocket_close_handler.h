// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_CLOSE_HANDLER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_CLOSE_HANDLER_H_

#include <memory>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/websocket_connection.h"
#include "net/test/embedded_test_server/websocket_handler.h"

namespace net::test_server {

// WebSocketCloseHandler is a handler for WebSocket connections that closes on
// receiving "Goodbye" and passively handles closing handshakes by returning the
// close code and reason.
class WebSocketCloseHandler : public WebSocketHandler {
 public:
  // Constructs the handler with a given WebSocket connection.
  explicit WebSocketCloseHandler(scoped_refptr<WebSocketConnection> connection);

  ~WebSocketCloseHandler() override;

  // Receives messages. Closes on "Goodbye" text.
  void OnTextMessage(std::string_view message) override;
};

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_CLOSE_HANDLER_H_
