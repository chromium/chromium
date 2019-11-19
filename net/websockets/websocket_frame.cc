// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_frame.h"

#include <stddef.h>
#include <algorithm>

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// GCC (and Clang) can transparently use vector ops. Only try to do this on
// architectures where we know it works, otherwise gcc will attempt to emulate
// the vector ops, which is unlikely to be efficient.
#if defined(COMPILER_GCC) &&                                          \
    (defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM_FAMILY)) && \
    !defined(OS_NACL)

using PackedMaskType = uint32_t __attribute__((vector_size(16)));

#else

using PackedMaskType = size_t;

#endif  // defined(COMPILER_GCC) &&
        // (defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM_FAMILY)) &&
        // !defined(OS_NACL)

const uint8_t kFinalBit = 0x80;
const uint8_t kReserved1Bit = 0x40;
const uint8_t kReserved2Bit = 0x20;
const uint8_t kReserved3Bit = 0x10;
const uint8_t kOpCodeMask = 0xF;
const uint8_t kMaskBit = 0x80;
const uint64_t kMaxPayloadLengthWithoutExtendedLengthField = 125;
const uint64_t kPayloadLengthWithTwoByteExtendedLengthField = 126;
const uint64_t kPayloadLengthWithEightByteExtendedLengthField = 127;

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

WebSocketFrameChunk::WebSocketFrameChunk() : final_chunk(false) {}

WebSocketFrameChunk::~WebSocketFrameChunk() = default;

int GetWebSocketFrameHeaderSize(const WebSocketFrameHeader& header) {
  int extended_length_size = 0;
  if (header.payload_length > kMaxPayloadLengthWithoutExtendedLengthField &&
      header.payload_length <= UINT16_MAX) {
    extended_length_size = 2;
  } else if (header.payload_length > UINT16_MAX) {
    extended_length_size = 8;
  }

  return (WebSocketFrameHeader::kBaseHeaderSize + extended_length_size +
          (header.masked ? WebSocketFrameHeader::kMaskingKeyLength : 0));
}

int WriteWebSocketFrameHeader(const WebSocketFrameHeader& header,
                              const WebSocketMaskingKey* masking_key,
                              char* buffer,
                              int buffer_size) {
  DCHECK((header.opcode & kOpCodeMask) == header.opcode)
      << "header.opcode must fit to kOpCodeMask.";
  DCHECK(header.payload_length <= static_cast<uint64_t>(INT64_MAX))
      << "WebSocket specification doesn't allow a frame longer than "
      << "INT64_MAX (0x7FFFFFFFFFFFFFFF) bytes.";
  DCHECK_GE(buffer_size, 0);

  // WebSocket frame format is as follows:
  // - Common header (2 bytes)
  // - Optional extended payload length
  //   (2 or 8 bytes, present if actual payload length is more than 125 bytes)
  // - Optional masking key (4 bytes, present if MASK bit is on)
  // - Actual payload (XOR masked with masking key if MASK bit is on)
  //
  // This function constructs frame header (the first three in the list
  // above).

  int header_size = GetWebSocketFrameHeaderSize(header);
  if (header_size > buffer_size)
    return ERR_INVALID_ARGUMENT;

  int buffer_index = 0;

  uint8_t first_byte = 0u;
  first_byte |= header.final ? kFinalBit : 0u;
  first_byte |= header.reserved1 ? kReserved1Bit : 0u;
  first_byte |= header.reserved2 ? kReserved2Bit : 0u;
  first_byte |= header.reserved3 ? kReserved3Bit : 0u;
  first_byte |= header.opcode & kOpCodeMask;
  buffer[buffer_index++] = first_byte;

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
  buffer[buffer_index++] = second_byte;

  // Writes "extended payload length" field.
  if (extended_length_size == 2) {
    uint16_t payload_length_16 = static_cast<uint16_t>(header.payload_length);
    base::WriteBigEndian(buffer + buffer_index, payload_length_16);
    buffer_index += sizeof(payload_length_16);
  } else if (extended_length_size == 8) {
    base::WriteBigEndian(buffer + buffer_index, header.payload_length);
    buffer_index += sizeof(header.payload_length);
  }

  // Writes "masking key" field, if needed.
  if (header.masked) {
    DCHECK(masking_key);
    std::copy(masking_key->key,
              masking_key->key + WebSocketFrameHeader::kMaskingKeyLength,
              buffer + buffer_index);
    buffer_index += WebSocketFrameHeader::kMaskingKeyLength;
  } else {
    DCHECK(!masking_key);
  }

  DCHECK_EQ(header_size, buffer_index);
  return header_size;
}

WebSocketMaskingKey GenerateWebSocketMaskingKey() {
  // Masking keys should be generated from a cryptographically secure random
  // number generator, which means web application authors should not be able
  // to guess the next value of masking key.
  WebSocketMaskingKey masking_key;
  base::RandBytes(masking_key.key, WebSocketFrameHeader::kMaskingKeyLength);
  return masking_key;
}

void MaskWebSocketFramePayload(const WebSocketMaskingKey& masking_key,
                               uint64_t frame_offset,
                               char* const data,
                               int data_size) {
  static const size_t kMaskingKeyLength =
      WebSocketFrameHeader::kMaskingKeyLength;

  DCHECK_GE(data_size, 0);

  // Most of the masking is done in chunks of sizeof(PackedMaskType), except for
  // the beginning and the end of the buffer which may be unaligned.
  // PackedMaskType must be a multiple of kMaskingKeyLength in size.
  PackedMaskType packed_mask_key;
  static const size_t kPackedMaskKeySize = sizeof(packed_mask_key);
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
