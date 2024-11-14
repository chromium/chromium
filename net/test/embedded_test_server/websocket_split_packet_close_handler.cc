// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/websocket_split_packet_close_handler.h"

#include <memory>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "net/test/embedded_test_server/create_websocket_handler.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/websocket_connection.h"
#include "net/test/embedded_test_server/websocket_handler.h"
#include "net/websockets/websocket_frame.h"

namespace net::test_server {

WebSocketSplitPacketCloseHandler::WebSocketSplitPacketCloseHandler(
    scoped_refptr<WebSocketConnection> connection)
    : WebSocketHandler(std::move(connection)) {}

void WebSocketSplitPacketCloseHandler::OnClosingHandshake(
    std::optional<uint16_t> code,
    std::string_view message) {
  // Send the split close frame as a response to the client-initiated close.
  SendSplitCloseFrame();
}

void WebSocketSplitPacketCloseHandler::SendSplitCloseFrame() {
  static constexpr uint16_t kCode = 3004;
  static constexpr std::string_view kReason = "split test";

  const auto close_frame = CreateCloseFrame(kCode, kReason);

  // Split the close frame into two parts and send each separately.
  const auto close_frame_span = close_frame->span();

  const size_t split_index = 1;  // Split after the first byte
  connection()->SendRaw(close_frame_span.subspan(0, split_index));
  connection()->SendRaw(close_frame_span.subspan(split_index));
  connection()->DisconnectAfterAnyWritesDone();
}

EmbeddedTestServer::HandleUpgradeRequestCallback
WebSocketSplitPacketCloseHandler::CreateHandler() {
  return CreateWebSocketHandler(
      "/close-with-split-packet",
      base::BindRepeating([](scoped_refptr<WebSocketConnection> connection)
                              -> std::unique_ptr<WebSocketHandler> {
        return std::make_unique<WebSocketSplitPacketCloseHandler>(connection);
      }));
}

}  // namespace net::test_server
