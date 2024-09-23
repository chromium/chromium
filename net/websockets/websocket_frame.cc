// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_frame.h"

#include <stddef.h>
#include <string.h>

#include <ostream>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// GCC (and Clang) can transparently use vector ops. Only try to do this on
// architectures where we know it works, otherwise gcc will attempt to emulate
// the vector ops, which is unlikely to be efficient.
#if defined(COMPILER_GCC) && \
    (defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM_FAMILY))

using PackedMaskType = uint32_t __attribute__((vector_size(16)));

#else

using PackedMaskType = size_t;

#endif  // defined(COMPILER_GCC) &&
        // (defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM_FAMILY))

constexpr uint8_t kFinalBit = 0x80;
constexpr uint8_t kReserved1Bit = 0x40;
constexpr uint8_t kReserved2Bit = 0x20;
constexpr uint8_t kReserved3Bit = 0x10;
constexpr uint8_t kOpCodeMask = 0xF;
constexpr uint8_t kMaskBit = 0x80;
constexpr uint64_t kMaxPayloadLengthWithoutExtendedLengthField = 125;
constexpr uint64_t kPayloadLengthWithTwoByteExtendedLengthField = 126;
constexpr uint64_t kPayloadLengthWithEightByteExtendedLengthField = 127;

inline void MaskWebSocketFramePayloadByBytes(
    const WebSocketMaskingKey& masking_key,
    size_t masking_key_offset,
    const base::span<uint8_t> payload) {
  uint8_t* data = payload.data();
  const size_t size = payload.size();
  for (size_t i = 0; i < size; ++i) {
    // SAFETY: Performance sensitive. `data` is within `payload` bounds.
    UNSAFE_BUFFERS(data[i]) ^=
        masking_key.key[masking_key_offset++ %
                        WebSocketFrameHeader::kMaskingKeyLength];
  }
}

}  // namespace

std::unique_ptr<WebSocketFrameHeader> WebSocketFrameHeader::Clone() const {
  auto ret = std::make_unique<WebSocketFrameHeader>(opcode);
  ret->CopyFrom(*this);
  return ret;
}

void WebSocketFrameHeader::CopyFrom(const WebSocketFrameHeader& source) {
  final = source.final;
  reserved1 = source.reserved1;
  reserved2 = source.reserved2;
  reserved3 = source.reserved3;
  opcode = source.opcode;
  masked = source.masked;
  masking_key = source.masking_key;
  payload_length = source.payload_length;
}

WebSocketFrame::WebSocketFrame(WebSocketFrameHeader::OpCode opcode)
    : header(opcode) {}

WebSocketFrame::~WebSocketFrame() = default;

WebSocketFrameChunk::WebSocketFrameChunk() = default;

WebSocketFrameChunk::~WebSocketFrameChunk() = default;

size_t GetWebSocketFrameHeaderSize(const WebSocketFrameHeader& header) {
  size_t extended_length_size = 0u;
  if (header.payload_length > kMaxPayloadLengthWithoutExtendedLengthField &&
      header.payload_length <= UINT16_MAX) {
    extended_length_size = 2u;
  } else if (header.payload_length > UINT16_MAX) {
    extended_length_size = 8u;
  }

  return (WebSocketFrameHeader::kBaseHeaderSize + extended_length_size +
          (header.masked ? WebSocketFrameHeader::kMaskingKeyLength : 0u));
}

int WriteWebSocketFrameHeader(const WebSocketFrameHeader& header,
                              const WebSocketMaskingKey* masking_key,
                              base::span<uint8_t> buffer) {
  DCHECK((header.opcode & kOpCodeMask) == header.opcode)
      << "header.opcode must fit to kOpCodeMask.";
  DCHECK(header.payload_length <= static_cast<uint64_t>(INT64_MAX))
      << "WebSocket specification doesn't allow a frame longer than "
      << "INT64_MAX (0x7FFFFFFFFFFFFFFF) bytes.";

  // WebSocket frame format is as follows:
  // - Common header (2 bytes)
  // - Optional extended payload length
  //   (2 or 8 bytes, present if actual payload length is more than 125 bytes)
  // - Optional masking key (4 bytes, present if MASK bit is on)
  // - Actual payload (XOR masked with masking key if MASK bit is on)
  //
  // This function constructs frame header (the first three in the list
  // above).

  size_t header_size = GetWebSocketFrameHeaderSize(header);
  if (header_size > buffer.size()) {
    return ERR_INVALID_ARGUMENT;
  }

  base::SpanWriter writer(buffer);

  uint8_t first_byte = 0u;
  first_byte |= header.final ? kFinalBit : 0u;
  first_byte |= header.reserved1 ? kReserved1Bit : 0u;
  first_byte |= header.reserved2 ? kReserved2Bit : 0u;
  first_byte |= header.reserved3 ? kReserved3Bit : 0u;
  first_byte |= header.opcode & kOpCodeMask;
  writer.WriteU8BigEndian(first_byte);

  int extended_length_size = 0;
  uint8_t second_byte = 0u;
  second_byte |= header.masked ? kMaskBit : 0u;
  if (header.payload_length <= kMaxPayloadLengthWithoutExtendedLengthField) {
    second_byte |= header.payload_length;
  } else if (header.payload_length <= UINT16_MAX) {
    second_byte |= kPayloadLengthWithTwoByteExtendedLengthField;
    extended_length_size = 2;
  } else {
    second_byte |= kPayloadLengthWithEightByteExtendedLengthField;
    extended_length_size = 8;
  }
  writer.WriteU8BigEndian(second_byte);

  // Writes "extended payload length" field.
  if (extended_length_size == 2) {
    writer.WriteU16BigEndian(static_cast<uint16_t>(header.payload_length));
  } else if (extended_length_size == 8) {
    writer.WriteU64BigEndian(header.payload_length);
  }

  // Writes "masking key" field, if needed.
  if (header.masked) {
    DCHECK(masking_key);
    writer.Write(masking_key->key);
  } else {
    DCHECK(!masking_key);
  }

  // Verify we wrote the expected number of bytes.
  DCHECK_EQ(header_size, writer.num_written());
  return header_size;
}

WebSocketMaskingKey GenerateWebSocketMaskingKey() {
  // Masking keys should be generated from a cryptographically secure random
  // number generator, which means web application authors should not be able
  // to guess the next value of masking key.
  WebSocketMaskingKey masking_key;
  base::RandBytes(masking_key.key);
  return masking_key;
}

void MaskWebSocketFramePayload(const WebSocketMaskingKey& masking_key,
                               uint64_t frame_offset,
                               base::span<uint8_t> data) {
  static constexpr size_t kMaskingKeyLength =
      WebSocketFrameHeader::kMaskingKeyLength;

  // Most of the masking is done in chunks of sizeof(PackedMaskType), except for
  // the beginning and the end of the buffer which may be unaligned.
  // PackedMaskType must be a multiple of kMaskingKeyLength in size.
  PackedMaskType packed_mask_key;
  static constexpr size_t kPackedMaskKeySize = sizeof(packed_mask_key);
  static_assert((kPackedMaskKeySize >= kMaskingKeyLength &&
                 kPackedMaskKeySize % kMaskingKeyLength == 0),
                "PackedMaskType size is not a multiple of mask length");
  // If the buffer is too small for the vectorised version to be useful, revert
  // to the byte-at-a-time implementation early.
  if (data.size() <= kPackedMaskKeySize * 2) {
    MaskWebSocketFramePayloadByBytes(masking_key,
                                     frame_offset % kMaskingKeyLength, data);
    return;
  }
  const size_t data_modulus =
      reinterpret_cast<size_t>(data.data()) % kPackedMaskKeySize;
  auto [before_aligned, remaining] = data.split_at(
      data_modulus == 0 ? 0 : (kPackedMaskKeySize - data_modulus));
  auto [aligned, after_aligned] = remaining.split_at(
      remaining.size() - remaining.size() % kPackedMaskKeySize);
  MaskWebSocketFramePayloadByBytes(
      masking_key, frame_offset % kMaskingKeyLength, before_aligned);

  // Create a version of the mask which is rotated by the appropriate offset
  // for our alignment. The "trick" here is that 0 XORed with the mask will
  // give the value of the mask for the appropriate byte.
  std::array<uint8_t, kMaskingKeyLength> realigned_mask = {};
  MaskWebSocketFramePayloadByBytes(
      masking_key, (frame_offset + before_aligned.size()) % kMaskingKeyLength,
      base::as_writable_byte_span(realigned_mask));

  base::span<uint8_t> packed_span = base::byte_span_from_ref(packed_mask_key);
  while (!packed_span.empty()) {
    packed_span.copy_prefix_from(realigned_mask);
    packed_span = packed_span.subspan(realigned_mask.size());
  }

  // The main loop.
  while (!aligned.empty()) {
    // This is not quite standard-compliant C++. However, the standard-compliant
    // equivalent (using memcpy()) compiles to slower code using g++. In
    // practice, this will work for the compilers and architectures currently
    // supported by Chromium, and the tests are extremely unlikely to pass if a
    // future compiler/architecture breaks it.
    *reinterpret_cast<PackedMaskType*>(aligned.data()) ^= packed_mask_key;
    aligned = aligned.subspan(kPackedMaskKeySize);
  }

  MaskWebSocketFramePayloadByBytes(
      masking_key,
      (frame_offset + (data.size() - after_aligned.size())) % kMaskingKeyLength,
      after_aligned);
}

}  // namespace net
