// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_ECHO_REQUEST_HEADERS_HANDLER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_ECHO_REQUEST_HEADERS_HANDLER_H_

#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/websocket_connection.h"
#include "net/test/embedded_test_server/websocket_handler.h"

namespace net::test_server {

class WebSocketEchoRequestHeadersHandler : public WebSocketHandler {
 public:
  explicit WebSocketEchoRequestHeadersHandler(
      scoped_refptr<WebSocketConnection> connection);

  ~WebSocketEchoRequestHeadersHandler() override;

  // Handles the WebSocket handshake and retrieves headers.
  // Serializes the headers to JSON and sends it back to the client.
  void OnHandshake(const HttpRequest& request) override;
};

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_ECHO_REQUEST_HEADERS_HANDLER_H_
