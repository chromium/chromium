// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

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
    char* const begin,
    char* const end) {
  for (char* masked = begin; masked != end; ++masked) {
    *masked ^= masking_key.key[masking_key_offset++ %
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
                              char* buffer_ptr,
                              int buffer_size) {
  base::span<uint8_t> buffer = base::as_writable_bytes(
      // TODO(crbug.com/40284755): It's not possible to construct this span
      // soundedly here. WriteWebSocketFrameHeader() should receive a span
      // instead of a pointer and length.
      UNSAFE_BUFFERS(
          base::span(buffer_ptr, base::checked_cast<size_t>(buffer_size))));

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
                               char* const data,
                               int data_size) {
  static constexpr size_t kMaskingKeyLength =
      WebSocketFrameHeader::kMaskingKeyLength;

  DCHECK_GE(data_size, 0);

  // Most of the masking is done in chunks of sizeof(PackedMaskType), except for
  // the beginning and the end of the buffer which may be unaligned.
  // PackedMaskType must be a multiple of kMaskingKeyLength in size.
  PackedMaskType packed_mask_key;
  static constexpr size_t kPackedMaskKeySize = sizeof(packed_mask_key);
  static_assert((kPackedMaskKeySize >= kMaskingKeyLength &&
                 kPackedMaskKeySize % kMaskingKeyLength == 0),
                "PackedMaskType size is not a multiple of mask length");
  char* const end = data + data_size;
  // If the buffer is too small for the vectorised version to be useful, revert
  // to the byte-at-a-time implementation early.
  if (data_size <= static_cast<int>(kPackedMaskKeySize * 2)) {
    MaskWebSocketFramePayloadByBytes(
        masking_key, frame_offset % kMaskingKeyLength, data, end);
    return;
  }
  const size_t data_modulus =
      reinterpret_cast<size_t>(data) % kPackedMaskKeySize;
  char* const aligned_begin =
      data_modulus == 0 ? data : (data + kPackedMaskKeySize - data_modulus);
  // Guaranteed by the above check for small data_size.
  DCHECK(aligned_begin < end);
  MaskWebSocketFramePayloadByBytes(
      masking_key, frame_offset % kMaskingKeyLength, data, aligned_begin);
  const size_t end_modulus = reinterpret_cast<size_t>(end) % kPackedMaskKeySize;
  char* const aligned_end = end - end_modulus;
  // Guaranteed by the above check for small data_size.
  DCHECK(aligned_end > aligned_begin);
  // Create a version of the mask which is rotated by the appropriate offset
  // for our alignment. The "trick" here is that 0 XORed with the mask will
  // give the value of the mask for the appropriate byte.
  char realigned_mask[kMaskingKeyLength] = {};
  MaskWebSocketFramePayloadByBytes(
      masking_key,
      (frame_offset + aligned_begin - data) % kMaskingKeyLength,
      realigned_mask,
      realigned_mask + kMaskingKeyLength);

  for (size_t i = 0; i < kPackedMaskKeySize; i += kMaskingKeyLength) {
    // memcpy() is allegedly blessed by the C++ standard for type-punning.
    memcpy(reinterpret_cast<char*>(&packed_mask_key) + i,
           realigned_mask,
           kMaskingKeyLength);
  }

  // The main loop.
  for (char* merged = aligned_begin; merged != aligned_end;
       merged += kPackedMaskKeySize) {
    // This is not quite standard-compliant C++. However, the standard-compliant
    // equivalent (using memcpy()) compiles to slower code using g++. In
    // practice, this will work for the compilers and architectures currently
    // supported by Chromium, and the tests are extremely unlikely to pass if a
    // future compiler/architecture breaks it.
    *reinterpret_cast<PackedMaskType*>(merged) ^= packed_mask_key;
  }

  MaskWebSocketFramePayloadByBytes(
      masking_key,
      (frame_offset + (aligned_end - data)) % kMaskingKeyLength,
      aligned_end,
      end);
}

}  // namespace net
