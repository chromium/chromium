/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_FUNCTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_FUNCTIONS_H_

#include <stdint.h>
#include <type_traits>

#include "base/bit_cast.h"

namespace WTF {

namespace internal {

template <size_t size>
struct IntTypes;
template <>
struct IntTypes<1> {
  typedef uint8_t UnsignedType;
};
template <>
struct IntTypes<2> {
  typedef uint16_t UnsignedType;
};
template <>
struct IntTypes<4> {
  typedef uint32_t UnsignedType;
};
template <>
struct IntTypes<8> {
  typedef uint64_t UnsignedType;
};

template <typename T>
using IntHashBits = typename IntTypes<sizeof(T)>::UnsignedType;

// Hash functions for integral and enum types.

// Thomas Wang's 32 Bit Mix Function:
// https://web.archive.org/web/20060507103516/http://www.cris.com/~Ttwang/tech/inthash.htm
inline unsigned HashInt(uint32_t key) {
  key += ~(key << 15);
  key ^= (key >> 10);
  key += (key << 3);
  key ^= (key >> 6);
  key += ~(key << 11);
  key ^= (key >> 16);
  return key;
}

inline unsigned HashInt(uint16_t key16) {
  uint32_t key = key16;
  return HashInt(key);
}

inline unsigned HashInt(uint8_t key8) {
  uint32_t key = key8;
  return HashInt(key);
}

// Thomas Wang's 64 bit Mix Function:
// https://web.archive.org/web/20060507103516/http://www.cris.com/~Ttwang/tech/inthash.htm
inline unsigned HashInt(uint64_t key) {
  key += ~(key << 32);
  key ^= (key >> 22);
  key += ~(key << 13);
  key ^= (key >> 8);
  key += (key << 3);
  key ^= (key >> 15);
  key += ~(key << 27);
  key ^= (key >> 31);
  return static_cast<unsigned>(key);
}

}  // namespace internal

// Compound integer hash method:
// http://opendatastructures.org/versions/edition-0.1d/ods-java/node33.html#SECTION00832000000000000000
inline unsigned HashInts(unsigned key1, unsigned key2) {
  unsigned short_random1 = 277951225;          // A random 32-bit value.
  unsigned short_random2 = 95187966;           // A random 32-bit value.
  uint64_t long_random = 19248658165952623LL;  // A random, odd 64-bit value.

  uint64_t product =
      long_random * short_random1 * key1 + long_random * short_random2 * key2;
  unsigned high_bits = static_cast<unsigned>(
      product >> (8 * (sizeof(uint64_t) - sizeof(unsigned))));
  return high_bits;
}

template <typename T>
unsigned HashInt(T key) {
  static_assert(std::is_integral_v<T> || std::is_enum_v<T>);
  return internal::HashInt(static_cast<internal::IntHashBits<T>>(key));
}

template <typename T>
unsigned HashFloat(T key) {
  static_assert(std::is_floating_point_v<T>);
  return internal::HashInt(base::bit_cast<internal::IntHashBits<T>>(key));
}

template <typename T>
bool FloatEqualForHash(T a, T b) {
  static_assert(std::is_floating_point_v<T>);
  return base::bit_cast<internal::IntHashBits<T>>(a) ==
         base::bit_cast<internal::IntHashBits<T>>(b);
}

template <typename T>
unsigned HashPointer(T* key) {
  return HashInt(reinterpret_cast<internal::IntHashBits<T*>>(key));
}

// Useful compounding hash functions.
inline void AddIntToHash(unsigned& hash, unsigned key) {
  hash = ((hash << 5) + hash) + key;  // Djb2
}

inline void AddFloatToHash(unsigned& hash, float value) {
  AddIntToHash(hash, HashFloat(value));
}

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_HASH_FUNCTIONS_H_
