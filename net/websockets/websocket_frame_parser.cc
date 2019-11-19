// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_frame_parser.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/io_buffer.h"
#include "net/websockets/websocket_frame.h"

namespace {

const uint8_t kFinalBit = 0x80;
const uint8_t kReserved1Bit = 0x40;
const uint8_t kReserved2Bit = 0x20;
const uint8_t kReserved3Bit = 0x10;
const uint8_t kOpCodeMask = 0xF;
const uint8_t kMaskBit = 0x80;
const uint8_t kPayloadLengthMask = 0x7F;
const uint64_t kMaxPayloadLengthWithoutExtendedLengthField = 125;
const uint64_t kPayloadLengthWithTwoByteExtendedLengthField = 126;
const uint64_t kPayloadLengthWithEightByteExtendedLengthField = 127;
const size_t kMaximumFrameHeaderSize =
    net::WebSocketFrameHeader::kBaseHeaderSize +
    net::WebSocketFrameHeader::kMaximumExtendedLengthSize +
    net::WebSocketFrameHeader::kMaskingKeyLength;

}  // namespace.

namespace net {

WebSocketFrameParser::WebSocketFrameParser()
    : frame_offset_(0), websocket_error_(kWebSocketNormalClosure) {}

WebSocketFrameParser::~WebSocketFrameParser() = default;

bool WebSocketFrameParser::Decode(
    const char* data,
    size_t length,
    std::vector<std::unique_ptr<WebSocketFrameChunk>>* frame_chunks) {
  if (websocket_error_ != kWebSocketNormalClosure)
    return false;
  if (!length)
    return true;

  base::span<const char> data_span = base::make_span(data, length);
  // If we have incomplete frame header, try to decode a header combining with
  // |data|.
  bool first_chunk = false;
  if (incomplete_header_buffer_.size() > 0) {
    DCHECK(!current_frame_header_.get());
    const size_t original_size = incomplete_header_buffer_.size();
    DCHECK_LE(original_size, kMaximumFrameHeaderSize);
    incomplete_header_buffer_.insert(
        incomplete_header_buffer_.end(), data,
        data + std::min(length, kMaximumFrameHeaderSize - original_size));
    const size_t consumed = DecodeFrameHeader(incomplete_header_buffer_);
    if (websocket_error_ != kWebSocketNormalClosure)
      return false;
    if (!current_frame_header_.get())
      return true;

    DCHECK_GE(consumed, original_size);
    data_span = data_span.subspan(consumed - original_size);
    incomplete_header_buffer_.clear();
    first_chunk = true;
  }

  DCHECK(incomplete_header_buffer_.empty());
  while (data_span.size() > 0 || first_chunk) {
    if (!current_frame_header_.get()) {
      const size_t consumed = DecodeFrameHeader(data_span);
      if (websocket_error_ != kWebSocketNormalClosure)
        return false;
      // If frame header is incomplete, then carry over the remaining
      // data to the next round of Decode().
      if (!current_frame_header_.get()) {
        DCHECK(!consumed);
        incomplete_header_buffer_.insert(incomplete_header_buffer_.end(),
                                         data_span.data(),
                                         data_span.data() + data_span.size());
        // Sanity check: the size of carried-over data should not exceed
        // the maximum possible length of a frame header.
        DCHECK_LT(incomplete_header_buffer_.size(), kMaximumFrameHeaderSize);
        return true;
      }
      DCHECK_GE(data_span.size(), consumed);
      data_span = data_span.subspan(consumed);
      first_chunk = true;
    }
    DCHECK(incomplete_header_buffer_.empty());
    std::unique_ptr<WebSocketFrameChunk> frame_chunk =
        DecodeFramePayload(first_chunk, &data_span);
    first_chunk = false;
    DCHECK(frame_chunk.get());
    frame_chunks->push_back(std::move(frame_chunk));
  }
  return true;
}

size_t WebSocketFrameParser::DecodeFrameHeader(base::span<const char> data) {
  DVLOG(3) << "DecodeFrameHeader buffer size:"
           << ", data size:" << data.size();
  typedef WebSocketFrameHeader::OpCode OpCode;
  DCHECK(!current_frame_header_.get());

  // Header needs 2 bytes at minimum.
  if (data.size() < 2)
    return 0;
  size_t current = 0;
  const uint8_t first_byte = data[current++];
  const uint8_t second_byte = data[current++];

  const bool final = (first_byte & kFinalBit) != 0;
  const bool reserved1 = (first_byte & kReserved1Bit) != 0;
  const bool reserved2 = (first_byte & kReserved2Bit) != 0;
  const bool reserved3 = (first_byte & kReserved3Bit) != 0;
  const OpCode opcode = first_byte & kOpCodeMask;

  uint64_t payload_length = second_byte & kPayloadLengthMask;
  if (payload_length == kPayloadLengthWithTwoByteExtendedLengthField) {
    if (data.size() < current + 2)
      return 0;
    uint16_t payload_length_16;
    base::ReadBigEndian(&data[current], &payload_length_16);
    current += 2;
    payload_length = payload_length_16;
    if (payload_length <= kMaxPayloadLengthWithoutExtendedLengthField) {
      websocket_error_ = kWebSocketErrorProtocolError;
      return 0;
    }
  } else if (payload_length == kPayloadLengthWithEightByteExtendedLengthField) {
    if (data.size() < current + 8)
      return 0;
    base::ReadBigEndian(&data[current], &payload_length);
    current += 8;
    if (payload_length <= UINT16_MAX ||
        payload_length > static_cast<uint64_t>(INT64_MAX)) {
      websocket_error_ = kWebSocketErrorProtocolError;
      return 0;
    }
    if (payload_length > static_cast<uint64_t>(INT32_MAX)) {
      websocket_error_ = kWebSocketErrorMessageTooBig;
      return 0;
    }
  }
  DCHECK_EQ(websocket_error_, kWebSocketNormalClosure);

  WebSocketMaskingKey masking_key = {};
  const bool masked = (second_byte & kMaskBit) != 0;
  static const int kMaskingKeyLength = WebSocketFrameHeader::kMaskingKeyLength;
  if (masked) {
    if (data.size() < current + kMaskingKeyLength)
      return 0;
    std::copy(&data[current], &data[current] + kMaskingKeyLength,
              masking_key.key);
    current += kMaskingKeyLength;
  }

  current_frame_header_ = std::make_unique<WebSocketFrameHeader>(opcode);
  current_frame_header_->final = final;
  current_frame_header_->reserved1 = reserved1;
  current_frame_header_->reserved2 = reserved2;
  current_frame_header_->reserved3 = reserved3;
  current_frame_header_->masked = masked;
  current_frame_header_->masking_key = masking_key;
  current_frame_header_->payload_length = payload_length;
  DCHECK_EQ(0u, frame_offset_);
  return current;
}

std::unique_ptr<WebSocketFrameChunk> WebSocketFrameParser::DecodeFramePayload(
    bool first_chunk,
    base::span<const char>* data) {
  // The cast here is safe because |payload_length| is already checked to be
  // less than std::numeric_limits<int>::max() when the header is parsed.
  const int chunk_data_size = static_cast<int>(
      std::min(static_cast<uint64_t>(data->size()),
               current_frame_header_->payload_length - frame_offset_));

  auto frame_chunk = std::make_unique<WebSocketFrameChunk>();
  if (first_chunk) {
    frame_chunk->header = current_frame_header_->Clone();
  }
  frame_chunk->final_chunk = false;
  if (chunk_data_size > 0) {
    frame_chunk->payload = data->subspan(0, chunk_data_size);
    *data = data->subspan(chunk_data_size);
    frame_offset_ += chunk_data_size;
  }

  DCHECK_LE(frame_offset_, current_frame_header_->payload_length);
  if (frame_offset_ == current_frame_header_->payload_length) {
    frame_chunk->final_chunk = true;
    current_frame_header_.reset();
    frame_offset_ = 0;
  }

  return frame_chunk;
}

}  // namespace net
