// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_SPLIT_PACKET_CLOSE_HANDLER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_SPLIT_PACKET_CLOSE_HANDLER_H_

#include "base/memory/scoped_refptr.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/websocket_connection.h"
#include "net/test/embedded_test_server/websocket_handler.h"

namespace net::test_server {

// WebSocketSplitPacketCloseHandler sends a split close frame, mimicking
// the behavior of sending the frame in two parts with a code and message,
// after the client initiates the close handshake.
class WebSocketSplitPacketCloseHandler : public WebSocketHandler {
 public:
  // Constructs the handler with a given WebSocket connection.
  explicit WebSocketSplitPacketCloseHandler(
      scoped_refptr<WebSocketConnection> connection);

  // Overrides the close handshake response to send a split close frame.
  void OnClosingHandshake(std::optional<uint16_t> code,
                          std::string_view message) override;

 private:
  // Sends the split close frame in two parts.
  void SendSplitCloseFrame();
};

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_SPLIT_PACKET_CLOSE_HANDLER_H_
