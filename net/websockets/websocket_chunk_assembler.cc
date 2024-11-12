// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_chunk_assembler.h"

#include "base/compiler_specific.h"
#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/types/expected.h"
#include "net/base/net_errors.h"
#include "net/websockets/websocket_errors.h"
#include "net/websockets/websocket_frame.h"

namespace net {

namespace {

// This uses type uint64_t to match the definition of
// WebSocketFrameHeader::payload_length in websocket_frame.h.
constexpr uint64_t kMaxControlFramePayload = 125;

// Utility function to create a WebSocketFrame
std::unique_ptr<WebSocketFrame> MakeWebSocketFrame(
    const WebSocketFrameHeader& header,
    base::span<uint8_t> payload) {
  auto frame = std::make_unique<WebSocketFrame>(header.opcode);
  frame->header.CopyFrom(header);

  if (header.masked) {
    MaskWebSocketFramePayload(header.masking_key, 0, payload);
  }
  frame->payload = payload;

  return frame;
}

}  // namespace

WebSocketChunkAssembler::WebSocketChunkAssembler() = default;

WebSocketChunkAssembler::~WebSocketChunkAssembler() = default;

void WebSocketChunkAssembler::Reset() {
  current_frame_header_.reset();
  chunk_buffer_.clear();
  state_ = AssemblyState::kInitialFrame;
}

base::expected<std::unique_ptr<WebSocketFrame>, net::Error>
WebSocketChunkAssembler::HandleChunk(
    std::unique_ptr<WebSocketFrameChunk> chunk) {
  if (state_ == AssemblyState::kMessageFinished) {
    Reset();
  }

  if (chunk->header) {
    CHECK_EQ(state_, AssemblyState::kInitialFrame);
    CHECK(!current_frame_header_);
    current_frame_header_ = std::move(chunk->header);
  }

  CHECK(current_frame_header_);

  const WebSocketFrameHeader::OpCode opcode = current_frame_header_->opcode;
  const bool is_control_frame =
      WebSocketFrameHeader::IsKnownControlOpCode(opcode) ||
      WebSocketFrameHeader::IsReservedControlOpCode(opcode);
  const bool is_data_frame = WebSocketFrameHeader::IsKnownDataOpCode(opcode) ||
                             WebSocketFrameHeader::IsReservedDataOpCode(opcode);

  CHECK(is_control_frame || is_data_frame);

  if (is_control_frame && !current_frame_header_->final) {
    return base::unexpected(ERR_WS_PROTOCOL_ERROR);
  }

  if (is_control_frame &&
      current_frame_header_->payload_length > kMaxControlFramePayload) {
    return base::unexpected(ERR_WS_PROTOCOL_ERROR);
  }

  const bool is_first_chunk = state_ == AssemblyState::kInitialFrame;
  const bool is_final_chunk = chunk->final_chunk;

  const bool is_empty_middle_chunk =
      !is_first_chunk && !is_final_chunk && chunk->payload.empty();
  if (is_empty_middle_chunk) {
    return base::unexpected(ERR_IO_PENDING);
  }

  // Handle single-chunk frame without buffering
  const bool is_single_chunk_frame = is_first_chunk && is_final_chunk;
  if (is_single_chunk_frame) {
    CHECK_EQ(current_frame_header_->payload_length, chunk->payload.size());

    auto frame = MakeWebSocketFrame(*current_frame_header_,
                                    base::as_writable_bytes(chunk->payload));
    state_ = AssemblyState::kMessageFinished;
    return frame;
  }

  // For data frames, process each chunk separately without accumulating all
  // in memory (streaming to render process)
  if (is_data_frame) {
    auto frame = MakeWebSocketFrame(*current_frame_header_,
                                    base::as_writable_bytes(chunk->payload));

    // Since we are synthesizing a frame that the origin server didn't send,
    // we need to comply with the requirement ourselves.
    if (state_ == AssemblyState::kContinuationFrame) {
      // This is needed to satisfy the constraint of RFC7692:
      //
      //   An endpoint MUST NOT set the "Per-Message Compressed" bit of control
      //   frames and non-first fragments of a data message.
      frame->header.opcode = WebSocketFrameHeader::kOpCodeContinuation;
      frame->header.reserved1 = false;
      frame->header.reserved2 = false;
      frame->header.reserved3 = false;
    }
    frame->header.payload_length = chunk->payload.size();
    frame->header.final = current_frame_header_->final && chunk->final_chunk;

    if (is_final_chunk) {
      state_ = AssemblyState::kMessageFinished;
    } else {
      state_ = AssemblyState::kContinuationFrame;
    }

    return frame;
  }

  CHECK(is_control_frame && current_frame_header_->final);

  // Control frames should be processed as a unit as they are small in size.
  base::Extend(chunk_buffer_, chunk->payload);

  if (!chunk->final_chunk) {
    state_ = AssemblyState::kControlFrame;
    return base::unexpected(ERR_IO_PENDING);
  }
  state_ = AssemblyState::kMessageFinished;

  CHECK_EQ(current_frame_header_->payload_length, chunk_buffer_.size());

  auto frame = MakeWebSocketFrame(*current_frame_header_,
                                  base::as_writable_byte_span(chunk_buffer_));

  state_ = AssemblyState::kMessageFinished;
  return frame;
}

}  // namespace net
