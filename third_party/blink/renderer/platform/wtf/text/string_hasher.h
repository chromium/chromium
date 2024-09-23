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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_HASHER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_HASHER_H_

#ifdef __SSE2__
#include <emmintrin.h>
#elif defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#include <cstring>
#include <type_traits>

#include "base/dcheck_is_on.h"
#include "base/logging.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/rapidhash/rapidhash.h"

namespace WTF {

// This HashReader is for converting 16-bit strings to 8-bit strings,
// assuming that all characters are within Latin1 (i.e., the high bit
// is never set). In other words, using this gives exactly the same
// hash as if you took the 16-bit string, converted to LChar (removing
// every high byte; they must all be zero) and hashed using PlainHashReader.
// See the comment on PlainHashReader in rapidhash.h for more information.
struct ConvertTo8BitHashReader {
  static constexpr unsigned kCompressionFactor = 2;
  static constexpr unsigned kExpansionFactor = 1;

  ALWAYS_INLINE static uint64_t Read64(const uint8_t* ptr) {
    const uint16_t* p = reinterpret_cast<const uint16_t*>(ptr);
    DCHECK_LE(p[0], 0xff);
    DCHECK_LE(p[1], 0xff);
    DCHECK_LE(p[2], 0xff);
    DCHECK_LE(p[3], 0xff);
    DCHECK_LE(p[4], 0xff);
    DCHECK_LE(p[5], 0xff);
    DCHECK_LE(p[6], 0xff);
    DCHECK_LE(p[7], 0xff);
#ifdef __SSE2__
    __m128i x = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
    return _mm_cvtsi128_si64(_mm_packus_epi16(x, x));
#elif defined(__ARM_NEON__)
    uint16x8_t x;
    memcpy(&x, p, sizeof(x));
    return vget_lane_u64(vreinterpret_u64_u8(vmovn_u16(x)), 0);
#else
    return (uint64_t{p[0]}) | (uint64_t{p[1]} << 8) | (uint64_t{p[2]} << 16) |
           (uint64_t{p[3]} << 24) | (uint64_t{p[4]} << 32) |
           (uint64_t{p[5]} << 40) | (uint64_t{p[6]} << 48) |
           (uint64_t{p[7]} << 56);
#endif
  }
  ALWAYS_INLINE static uint64_t Read32(const uint8_t* ptr) {
    const uint16_t* p = reinterpret_cast<const uint16_t*>(ptr);
    DCHECK_LE(p[0], 0xff);
    DCHECK_LE(p[1], 0xff);
    DCHECK_LE(p[2], 0xff);
    DCHECK_LE(p[3], 0xff);
#ifdef __SSE2__
    __m128i x = _mm_loadu_si64(reinterpret_cast<const __m128i*>(p));
    return _mm_cvtsi128_si64(_mm_packus_epi16(x, x));
#elif defined(__ARM_NEON__)
    uint16x4_t x;
    memcpy(&x, p, sizeof(x));
    uint16x8_t x_wide = vcombine_u16(x, x);
    return vget_lane_u32(vreinterpret_u32_u8(vmovn_u16(x_wide)), 0);
#else
    return (uint64_t{p[0]}) | (uint64_t{p[1]} << 8) | (uint64_t{p[2]} << 16) |
           (uint64_t{p[3]} << 24);
#endif
  }
  ALWAYS_INLINE static uint64_t ReadSmall(const uint8_t* ptr, size_t k) {
    const uint16_t* p = reinterpret_cast<const uint16_t*>(ptr);
    DCHECK_LE(p[0], 0xff);
    DCHECK_LE(p[k >> 1], 0xff);
    DCHECK_LE(p[k - 1], 0xff);
    return (uint64_t{p[0]} << 56) | (uint64_t{p[k >> 1]} << 32) | p[k - 1];
  }
};

class StringHasher {
  DISALLOW_NEW();

 public:
  static const unsigned kFlagCount =
      8;  // Save 8 bits for StringImpl to use as flags.

  // The main entry point for the string hasher. Computes the hash and returns
  // only the lowest 24 bits, since that's what we have room for in StringImpl.
  //
  // NOTE: length is the number of bytes produced _by the reader_.
  // Normally, this means that the number of bytes actually read will be
  // equivalent to (length * Reader::kCompressionFactor /
  // Reader::kExpansionFactor). Also note that if you are hashing something
  // that is not 8-bit elements, and do _not_ use compression factors or
  // similar, you'll need to multiply by sizeof(T) to get all data read.
  template <class Reader = PlainHashReader>
  static unsigned ComputeHashAndMaskTop8Bits(const char* data,
                                             unsigned length) {
    return MaskTop8Bits(
        rapidhash<Reader>(reinterpret_cast<const uint8_t*>(data), length));
  }

  // Hashing can be very performance-sensitive, but the hashing function is also
  // fairly big (~300 bytes on x86-64, give or take). This function is exactly
  // equivalent to ComputeHashAndMaskTop8Bits(), except that it is marked as
  // ALWAYS_INLINE and thus will be force-inlined into your own code. You should
  // use this if all of these are true:
  //
  //   1. You are in a frequently-called place, i.e. you are performance
  //   sensitive.
  //   2. You frequently hash short strings, so that the function call overhead
  //      dominates; for hashing e.g. 1 kB of data, this makes no sense to call.
  //   3. The gain of increased performance, ideally demonstrated by benchmarks,
  //      outweighs the cost of the binary size increase.
  //
  // Note that the compiler may choose to inline even
  // ComputeHashAndMaskTop8Bits() if it deems it a win; for instance, if you
  // call it with length equivalent to a small constant known at compile time,
  // the function may be subject to dead-code removal and thus considered small
  // enough to inline. The same goes if you are the only user of your
  // HashReader.
  template <class Reader = PlainHashReader>
  ALWAYS_INLINE static unsigned ComputeHashAndMaskTop8BitsInline(
      const char* data,
      unsigned length) {
    return MaskTop8Bits(
        rapidhash<Reader>(reinterpret_cast<const uint8_t*>(data), length));
  }

  static unsigned HashMemory(const void* data, unsigned length) {
    // NOTE: We lose the upper 32 bits of the hash value due to the
    // return API here. Moving all callers to support 64-bit hashing
    // would probably be possible, but a bit of work.
    return static_cast<unsigned>(
        rapidhash(reinterpret_cast<const uint8_t*>(data), length));
  }

  template <size_t length>
  static unsigned HashMemory(const void* data) {
    return HashMemory(data, length);
  }

 private:
  static unsigned MaskTop8Bits(uint64_t result) {
    // Reserving space from the high bits for flags preserves most of the hash's
    // value, since hash lookup typically masks out the high bits anyway.
    result &= (1U << (32 - kFlagCount)) - 1;

    // This avoids ever returning a hash code of 0, since that is used to
    // signal "hash not computed yet". Setting the high bit maintains
    // reasonable fidelity to a hash code of 0 because it is likely to yield
    // exactly 0 when hash lookup masks out the high bits.
    if (!result) {
      result = 0x80000000 >> kFlagCount;
    }

    return static_cast<unsigned>(result);
  }
};

}  // namespace WTF

using WTF::StringHasher;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_HASHER_H_
