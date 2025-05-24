// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/websocket_split_packet_close_handler.h"

#include <memory>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
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

  // Split after the first byte
  const auto [first, rest] = close_frame_span.split_at<1>();
  connection()->SendRaw(first);
  connection()->SendRaw(rest);
  connection()->DisconnectAfterAnyWritesDone();
}

}  // namespace net::test_server
