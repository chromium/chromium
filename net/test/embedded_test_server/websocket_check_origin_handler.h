// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_CHECK_ORIGIN_HANDLER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_CHECK_ORIGIN_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/websocket_connection.h"
#include "net/test/embedded_test_server/websocket_handler.h"

namespace net::test_server {

// WebSocketCheckOriginHandler is a handler for WebSocket connections that
// echoes back the WebSocket origin to the sender once the handshake is
// complete. Useful for testing WebSocket origin policies.
class WebSocketCheckOriginHandler : public WebSocketHandler {
 public:
  // Constructs the handler with a given WebSocket connection.
  explicit WebSocketCheckOriginHandler(
      scoped_refptr<WebSocketConnection> connection);

  ~WebSocketCheckOriginHandler() override;

  // Accepts all WebSocket handshake requests and stores the origin.
  void OnHandshake(const HttpRequest& request) override;

  // Sends the stored WebSocket origin to the client after the handshake is
  // complete.
  void OnHandshakeComplete() override;

 private:
  // Stores the origin from the handshake request to echo after the handshake.
  std::string origin_;
};

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_CHECK_ORIGIN_HANDLER_H_
