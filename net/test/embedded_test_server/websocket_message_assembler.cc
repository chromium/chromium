// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/websocket_message_assembler.h"

#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "net/base/net_errors.h"

namespace net::test_server {

WebSocketMessageAssembler::WebSocketMessageAssembler() = default;
WebSocketMessageAssembler::~WebSocketMessageAssembler() = default;

MessageOrError WebSocketMessageAssembler::HandleFrame(
    bool is_final,
    WebSocketFrameHeader::OpCode opcode,
    base::span<const char> payload) {
  if (state_ == MessageState::kFinished) {
    Reset();
  }

  switch (opcode) {
    case WebSocketFrameHeader::kOpCodeText:
      if (state_ != MessageState::kIdle) {
        VLOG(1) << "Unexpected text frame while expecting continuation";
        return base::unexpected(ERR_WS_PROTOCOL_ERROR);
      }
      is_text_message_ = true;
      break;

    case WebSocketFrameHeader::kOpCodeBinary:
      if (state_ != MessageState::kIdle) {
        VLOG(1) << "Unexpected binary frame while expecting continuation";
        return base::unexpected(ERR_WS_PROTOCOL_ERROR);
      }
      // Explicitly set to indicate binary handling.
      is_text_message_ = false;
      break;

    case WebSocketFrameHeader::kOpCodeContinuation:
      if (state_ == MessageState::kIdle) {
        VLOG(1) << "Unexpected continuation frame in idle state";
        return base::unexpected(ERR_WS_PROTOCOL_ERROR);
      }
      break;

    default:
      VLOG(1) << "Invalid frame opcode: " << opcode;
      return base::unexpected(ERR_WS_PROTOCOL_ERROR);
  }

  // If it's the final frame and we haven't received previous fragments, return
  // the current payload directly as the message. This avoids using an internal
  // buffer, optimizing memory usage by eliminating unnecessary copies.
  if (is_final && multi_frame_buffer_.empty()) {
    return Message(is_text_message_, base::as_bytes(payload));
  }

  base::Extend(multi_frame_buffer_, base::as_byte_span(payload));

  if (is_final) {
    Message complete_message(is_text_message_, base::span(multi_frame_buffer_));
    state_ = MessageState::kFinished;
    return complete_message;
  }

  // Update the state to expect a continuation frame.
  state_ = is_text_message_ ? MessageState::kExpectTextContinuation
                            : MessageState::kExpectBinaryContinuation;
  return base::unexpected(ERR_IO_PENDING);
}

void WebSocketMessageAssembler::Reset() {
  multi_frame_buffer_.clear();
  state_ = MessageState::kIdle;
  is_text_message_ = false;
}

}  // namespace net::test_server
