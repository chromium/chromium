// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_MESSAGE_ASSEMBLER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_MESSAGE_ASSEMBLER_H_

#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "base/types/expected.h"
#include "net/base/net_errors.h"
#include "net/websockets/websocket_frame.h"

namespace net::test_server {

struct Message {
  bool is_text_message;
  // `body` either points into the `payload` passed to `HandleFrame()` or into
  // storage owned by the WebSocketMessageAssembler object. It will be
  // invalidated by the next call to `HandleFrame()`.
  base::raw_span<const uint8_t> body;
};

using MessageOrError = base::expected<Message, net::Error>;

class WebSocketMessageAssembler final {
 public:
  WebSocketMessageAssembler();
  ~WebSocketMessageAssembler();

  // Handles incoming WebSocket frames and assembles messages.
  // If `final` is true and the message is complete, it returns a `Message`.
  // If more frames are expected, it returns `ERR_IO_PENDING`.
  // Possible errors are `ERR_IO_PENDING` and `ERR_WS_PROTOCOL_ERROR`.
  // Note: Validation of text messages as UTF-8 is the responsibility of the
  // client.
  MessageOrError HandleFrame(bool final,
                             WebSocketFrameHeader::OpCode opcode,
                             base::span<const char> payload);

  // Resets internal state when a message is fully processed or in case of
  // errors.
  void Reset();

 private:
  // Buffer to hold partial frames for multi-frame messages.
  std::vector<uint8_t> multi_frame_buffer_;

  // State to track if we are expecting a continuation frame.
  enum class MessageState {
    // kIdle: No message is being processed.
    kIdle,
    kExpectTextContinuation,
    kExpectBinaryContinuation,
    kFinished
  } state_ = MessageState::kIdle;

  bool is_text_message_ = false;
};
}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_WEBSOCKET_MESSAGE_ASSEMBLER_H_
