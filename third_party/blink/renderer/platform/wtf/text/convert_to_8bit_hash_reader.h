/*
 * Copyright (C) 2005, 2006, 2008, 2010, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_CONVERT_TO_8BIT_HASH_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_CONVERT_TO_8BIT_HASH_READER_H_

#ifdef __SSE2__
#include <emmintrin.h>
#elif defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#include <array>
#include <cstdint>

#include "base/bit_cast.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"

namespace blink {

// This HashReader is for converting 16-bit strings to 8-bit strings,
// assuming that all characters are within Latin1 (i.e., the high bit
// is never set). In other words, using this gives exactly the same
// hash as if you took the 16-bit string, converted to LChar (removing
// every high byte; they must all be zero) and hashed using PlainHashReader.
// See the comment on PlainHashReader in rapidhash.h for more information.
struct ConvertTo8BitHashReader {
  static constexpr size_t kCompressionFactor = 2;
  static constexpr size_t kExpansionFactor = 1;

  // SAFETY: rapidhash provides at least 16 readable bytes.
  UNSAFE_BUFFER_USAGE ALWAYS_INLINE static uint64_t Read64(const uint8_t* ptr) {
    return Read64(base::span<const uint8_t, 16>(base::unchecked, ptr, 16u));
  }

  ALWAYS_INLINE static uint64_t Read64(base::span<const uint8_t, 16> bytes) {
    const std::array<uint64_t, 2> buffer = {
        base::U64FromNativeEndian(bytes.first<8>()),
        base::U64FromNativeEndian(bytes.last<8>())};
    const auto p = base::bit_cast<std::array<uint16_t, 8>>(buffer);
    DCHECK_LE(p[0], 0xff);
    DCHECK_LE(p[1], 0xff);
    DCHECK_LE(p[2], 0xff);
    DCHECK_LE(p[3], 0xff);
    DCHECK_LE(p[4], 0xff);
    DCHECK_LE(p[5], 0xff);
    DCHECK_LE(p[6], 0xff);
    DCHECK_LE(p[7], 0xff);
#ifdef __SSE2__
    __m128i x = base::bit_cast<__m128i>(p);
    return _mm_cvtsi128_si64(_mm_packus_epi16(x, x));
#elif defined(__ARM_NEON__)
    uint16x8_t x = base::bit_cast<uint16x8_t>(p);
    return vget_lane_u64(vreinterpret_u64_u8(vmovn_u16(x)), 0);
#else
    return (uint64_t{p[0]}) | (uint64_t{p[1]} << 8) | (uint64_t{p[2]} << 16) |
           (uint64_t{p[3]} << 24) | (uint64_t{p[4]} << 32) |
           (uint64_t{p[5]} << 40) | (uint64_t{p[6]} << 48) |
           (uint64_t{p[7]} << 56);
#endif
  }

  // SAFETY: rapidhash provides at least 8 readable bytes.
  UNSAFE_BUFFER_USAGE ALWAYS_INLINE static uint64_t Read32(const uint8_t* ptr) {
    return Read32(base::span<const uint8_t, 8>(base::unchecked, ptr, 8u));
  }

  ALWAYS_INLINE static uint64_t Read32(base::span<const uint8_t, 8> bytes) {
    const auto p = base::bit_cast<std::array<uint16_t, 4>>(
        base::U64FromNativeEndian(bytes));
    DCHECK_LE(p[0], 0xff);
    DCHECK_LE(p[1], 0xff);
    DCHECK_LE(p[2], 0xff);
    DCHECK_LE(p[3], 0xff);
#ifdef __SSE2__
    const std::array<uint64_t, 2> padded = {base::bit_cast<uint64_t>(p), 0};
    __m128i x = base::bit_cast<__m128i>(padded);
    return _mm_cvtsi128_si64(_mm_packus_epi16(x, x));
#elif defined(__ARM_NEON__)
    uint16x4_t x = base::bit_cast<uint16x4_t>(p);
    uint16x8_t x_wide = vcombine_u16(x, x);
    return vget_lane_u32(vreinterpret_u32_u8(vmovn_u16(x_wide)), 0);
#else
    return (uint64_t{p[0]}) | (uint64_t{p[1]} << 8) | (uint64_t{p[2]} << 16) |
           (uint64_t{p[3]} << 24);
#endif
  }

  // SAFETY: rapidhash callback.
  UNSAFE_BUFFER_USAGE ALWAYS_INLINE static uint64_t ReadSmall(
      const uint8_t* ptr,
      size_t k) {
    const uint16_t* p = reinterpret_cast<const uint16_t*>(ptr);
    DCHECK_LE(p[0], 0xff);
    DCHECK_LE(p[k >> 1], 0xff);
    DCHECK_LE(p[k - 1], 0xff);
    return (uint64_t{p[0]} << 56) | (uint64_t{p[k >> 1]} << 32) | p[k - 1];
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_CONVERT_TO_8BIT_HASH_READER_H_
