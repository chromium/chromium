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


#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_HASHER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_HASHER_H_

#include <stdint.h>

#include <cstring>

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/rapidhash/rapidhash.h"

namespace blink {

class StringHasher {
  DISALLOW_NEW();

 public:
  static const unsigned kFlagCount =
      8;  // Save 8 bits for StringImpl to use as flags.

  // The main entry point for the string hasher. Computes the hash and returns
  // only the lowest 24 bits, since that's what we have room for in StringImpl.
  template <class Reader = PlainHashReader>
  static uint32_t ComputeHashAndMaskTop8Bits(base::span<const uint8_t> data) {
    return MaskTop8Bits(rapidhash<Reader>(
        data.data(),
        data.size() * Reader::kExpansionFactor / Reader::kCompressionFactor));
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
  ALWAYS_INLINE static uint32_t ComputeHashAndMaskTop8BitsInline(
      base::span<const uint8_t> data) {
    return MaskTop8Bits(rapidhash<Reader>(
        data.data(),
        data.size() * Reader::kExpansionFactor / Reader::kCompressionFactor));
  }

 private:
  static uint32_t MaskTop8Bits(uint64_t result) {
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

    return static_cast<uint32_t>(result);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_HASHER_H_
