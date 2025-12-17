// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_ASCII_LOWER_HASH_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_ASCII_LOWER_HASH_READER_H_

#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"

namespace blink {

// This class is a reader that converts ASCII upper-case characters to
// lower-case. This is to be used as a character reader for StringHasher.
//
// NOTE: Interestingly, the SIMD paths here improve on code size, not just
// on performance.
template <typename CharType>
struct AsciiLowerHashReader {
  static constexpr unsigned kCompressionFactor = 1;
  static constexpr unsigned kExpansionFactor = 1;

  ALWAYS_INLINE static uint64_t Lowercase(CharType ch) {
    return ToASCIILower(ch);
  }

  // SAFETY: rapidhash callback.
  UNSAFE_BUFFER_USAGE ALWAYS_INLINE static uint64_t Read64(const uint8_t* ptr) {
    const CharType* p = reinterpret_cast<const CharType*>(ptr);
#if defined(__SSE2__) || defined(__ARM_NEON__)
    CharType b __attribute__((vector_size(8)));
    memcpy(&b, p, sizeof(b));
    b |= (b >= 'A' & b <= 'Z') & 0x20;
    uint64_t ret;
    memcpy(&ret, &b, sizeof(b));
    return ret;
#else
    if constexpr (sizeof(CharType) == 2) {
      return Lowercase(p[0]) | (Lowercase(p[1]) << 16) |
             (Lowercase(p[2]) << 32) | (Lowercase(p[3]) << 48);
    } else {
      return Lowercase(p[0]) | (Lowercase(p[1]) << 8) |
             (Lowercase(p[2]) << 16) | (Lowercase(p[3]) << 24) |
             (Lowercase(p[4]) << 32) | (Lowercase(p[5]) << 40) |
             (Lowercase(p[6]) << 48) | (Lowercase(p[7]) << 56);
    }
#endif
  }

  // SAFETY: rapidhash callback.
  UNSAFE_BUFFER_USAGE ALWAYS_INLINE static uint64_t Read32(const uint8_t* ptr) {
    const CharType* p = reinterpret_cast<const CharType*>(ptr);
#if defined(__SSE2__) || defined(__ARM_NEON__)
    CharType b __attribute__((vector_size(4)));
    memcpy(&b, p, sizeof(b));
    b |= (b >= 'A' & b <= 'Z') & 0x20;
    uint32_t ret;
    memcpy(&ret, &b, sizeof(b));
    return ret;
#else
    if constexpr (sizeof(CharType) == 2) {
      return Lowercase(p[0]) | (Lowercase(p[1]) << 16);
    } else {
      return Lowercase(p[0]) | (Lowercase(p[1]) << 8) |
             (Lowercase(p[2]) << 16) | (Lowercase(p[3]) << 24);
    }
#endif
  }

  // SAFETY: rapidhash callback.
  UNSAFE_BUFFER_USAGE ALWAYS_INLINE static uint64_t ReadSmall(
      const uint8_t* ptr,
      size_t size) {
    if constexpr (sizeof(CharType) == 2) {
      // This is fine, but the reasoning is a bit subtle. If we get here,
      // we have to be a UTF-16 string, and since ReadSmall can only be called
      // with 1, 2 or 3, it means we must be a UTF-16 string with a single
      // code point (i.e., two bytes). Furthermore, we know that this code point
      // must be above 0xFF, or the HashTranslatorLowercaseBuffer constructor
      // would not have called us. Thus, ToASCIILower() on this code point would
      // do nothing, and this, we should just hash it exactly as PlainHashReader
      // would have done.
      DCHECK_EQ(size, 2u);
      size = 2;
      return (uint64_t{ptr[0]} << 56) | (uint64_t{ptr[size >> 1]} << 32) |
             uint64_t{ptr[size - 1]};
    } else {
      return (Lowercase(ptr[0]) << 56) | (Lowercase(ptr[size >> 1]) << 32) |
             Lowercase(ptr[size - 1]);
    }
  }
};

// Combines AsciiLowerHashReader and ConvertTo8BitHashReader into one.
// This is an obscure case that we only need for completeness,
// so it is fine that it's not all that optimized.
struct AsciiConvertTo8AndLowerHashReader {
  static constexpr unsigned kCompressionFactor = 2;
  static constexpr unsigned kExpansionFactor = 1;

  static uint64_t Lowercase(uint16_t ch) { return ToASCIILower(ch); }

  // SAFETY: rapidhash callback.
  UNSAFE_BUFFER_USAGE static uint64_t Read64(const uint8_t* ptr) {
    const uint16_t* p = reinterpret_cast<const uint16_t*>(ptr);
    return Lowercase(p[0]) | (Lowercase(p[1]) << 8) | (Lowercase(p[2]) << 16) |
           (Lowercase(p[3]) << 24) | (Lowercase(p[4]) << 32) |
           (Lowercase(p[5]) << 40) | (Lowercase(p[6]) << 48) |
           (Lowercase(p[7]) << 56);
  }

  // SAFETY: rapidhash callback.
  UNSAFE_BUFFER_USAGE static uint64_t Read32(const uint8_t* ptr) {
    const uint16_t* p = reinterpret_cast<const uint16_t*>(ptr);
    return Lowercase(p[0]) | (Lowercase(p[1]) << 8) | (Lowercase(p[2]) << 16) |
           (Lowercase(p[3]) << 24);
  }

  // SAFETY: rapidhash callback.
  UNSAFE_BUFFER_USAGE static uint64_t ReadSmall(const uint8_t* ptr,
                                                size_t size) {
    const uint16_t* p = reinterpret_cast<const uint16_t*>(ptr);
    return (Lowercase(p[0]) << 56) | (Lowercase(p[size >> 1]) << 32) |
           Lowercase(p[size - 1]);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_ASCII_LOWER_HASH_READER_H_
