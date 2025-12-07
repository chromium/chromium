// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_SET_HSTS_HANDLER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_SET_HSTS_HANDLER_H_

#include "base/memory/scoped_refptr.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/websocket_handler.h"

namespace net::test_server {

class WebSocketConnection;

// WebSocketSetHstsHandler is a handler for WebSocket connections that enables
// HSTS for the host.
class WebSocketSetHstsHandler : public WebSocketHandler {
 public:
  // Constructs the handler with a given WebSocket connection.
  explicit WebSocketSetHstsHandler(
      scoped_refptr<WebSocketConnection> connection);

  void OnHandshake(const HttpRequest& request) override;
};

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_SET_HSTS_HANDLER_H_
