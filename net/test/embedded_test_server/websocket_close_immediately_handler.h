// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_CLOSE_IMMEDIATELY_HANDLER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_CLOSE_IMMEDIATELY_HANDLER_H_

#include "base/memory/scoped_refptr.h"
#include "net/test/embedded_test_server/websocket_connection.h"
#include "net/test/embedded_test_server/websocket_handler.h"

namespace net::test_server {

// WebSocketCloseImmediatelyHandler is a handler for WebSocket connections that
// cleanly closes the WebSocket immediately after receiving a handshake.
class WebSocketCloseImmediatelyHandler
    : public net::test_server::WebSocketHandler {
 public:
  explicit WebSocketCloseImmediatelyHandler(
      scoped_refptr<net::test_server::WebSocketConnection> connection);
  ~WebSocketCloseImmediatelyHandler() override;

  void OnHandshakeComplete() override;
};

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_CLOSE_IMMEDIATELY_HANDLER_H_
