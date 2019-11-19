// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_RECOVER_MODULE_INTEGERS_H_
#define SQL_RECOVER_MODULE_INTEGERS_H_

#include <cstdint>
#include <limits>
#include <utility>

namespace sql {
namespace recover {

// Reads an unsigned 16-bit big-endian integer from the given buffer.
//
// |buffer| must point to at least two consecutive bytes of valid memory.
//
// The return type was chosen because it suits the callers best.
inline int LoadBigEndianUint16(const uint8_t* buffer) {
  static_assert(
      std::numeric_limits<uint16_t>::max() <= std::numeric_limits<int>::max(),
      "The value may overflow the return type");
  return (static_cast<int>(buffer[0]) << 8) | static_cast<int>(buffer[1]);
}

// Reads a signed 32-bit big-endian integer from the given buffer.
//
// |buffer| must point to at least four consecutive bytes of valid memory.
inline int32_t LoadBigEndianInt32(const uint8_t* buffer) {
  // The code gets optimized to mov + bswap on x86_64, and to ldr + rev on ARM.
  return (static_cast<int32_t>(buffer[0]) << 24) |
         (static_cast<int32_t>(buffer[1]) << 16) |
         (static_cast<int32_t>(buffer[2]) << 8) |
         static_cast<int32_t>(buffer[3]);
}

// Reads a signed 64-bit big-endian integer from the given buffer.
//
// |buffer| must point to at least eight consecutive bytes of valid memory.
inline int64_t LoadBigEndianInt64(const uint8_t* buffer) {
  // The code gets optimized to mov + bswap on x86_64, and to ldr + rev on ARM.
  return (static_cast<int64_t>(buffer[0]) << 56) |
         (static_cast<int64_t>(buffer[1]) << 48) |
         (static_cast<int64_t>(buffer[2]) << 40) |
         (static_cast<int64_t>(buffer[3]) << 32) |
         (static_cast<int64_t>(buffer[4]) << 24) |
         (static_cast<int64_t>(buffer[5]) << 16) |
         (static_cast<int64_t>(buffer[6]) << 8) |
         static_cast<int64_t>(buffer[7]);
}

// Reads a SQLite varint.
//
// SQLite varints decode to 64-bit integers, and take up at most 9 bytes.
// If present, the 9th byte holds bits 56-63 of the integer. This deviates from
// Google (protobuf, leveldb) varint encoding, where the last varint byte's top
// bit is always 0.
//
// The implementation assumes that |buffer| and |buffer_end| point into the same
// array of bytes, and that |buffer| < |buffer_end|. The implementation will
// never compute a pointer value larger than |buffer_end|.
//
// Returns the parsed number and a pointer to the first byte past the number.
// Per the rules above, the returned pointer is guaranteed to be between
// |buffer| and |buffer_end|. The returned pointer is also guaranteed to be at
// most |kMaxVarintSize| bytes past |buffer|.
std::pair<int64_t, const uint8_t*> ParseVarint(const uint8_t* buffer,
                                               const uint8_t* buffer_end);

// The maximum number of bytes used to store a SQLite varint.
constexpr int kMaxVarintSize = 9;

}  // namespace recover
}  // namespace sql

#endif  // SQL_RECOVER_MODULE_INTEGERS_H_
