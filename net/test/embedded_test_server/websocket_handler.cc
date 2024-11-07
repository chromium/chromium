// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/websocket_handler.h"

#include "base/memory/scoped_refptr.h"
#include "net/test/embedded_test_server/websocket_connection.h"

namespace net::test_server {

WebSocketHandler::WebSocketHandler(
    scoped_refptr<WebSocketConnection> connection)
    : connection_(connection) {}

WebSocketHandler::~WebSocketHandler() = default;

// Default implementation of OnPing that responds with a PONG message.
void WebSocketHandler::OnPing(base::span<const uint8_t> payload) {
  if (connection()) {
    connection()->SendPong(payload);
  }
}

// Default implementation of OnPong that does nothing.
void WebSocketHandler::OnPong(base::span<const uint8_t> payload) {
  // Default implementation does nothing.
  // Optionally, you could log the receipt of the PONG message.
  DVLOG(3) << "Received PONG message.";
}

}  // namespace net::test_server
