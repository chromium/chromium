// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef COMPONENTS_ZUCCHINI_PATCH_UTILS_H_
#define COMPONENTS_ZUCCHINI_PATCH_UTILS_H_

#include <stdint.h>

#include <iterator>
#include <type_traits>

#include "components/zucchini/image_utils.h"
#include "components/zucchini/version_info.h"

namespace zucchini {

// A Zucchini 'ensemble' patch is the concatenation of a patch header with a
// list of patch 'elements', each containing data for patching individual
// elements.

// Supported by MSVC, g++, and clang++. Ensures no gaps in packing.
#pragma pack(push, 1)

// Header for a Zucchini patch, found at the beginning of an ensemble patch.
struct PatchHeader {
  // Magic signature at the beginning of a Zucchini patch file.
  enum : uint32_t { kMagic = 'Z' | ('u' << 8) | ('c' << 16) | ('c' << 24) };

  uint32_t magic = 0;
  uint16_t major_version = kInvalidVersion;
  uint16_t minor_version = kInvalidVersion;
  uint32_t old_size = 0;
  uint32_t old_crc = 0;
  uint32_t new_size = 0;
  uint32_t new_crc = 0;
};

// Sanity check.
static_assert(sizeof(PatchHeader) == 24, "PatchHeader must be 24 bytes");

// Header for a patch element, found at the beginning of every patch element.
struct PatchElementHeader {
  uint32_t old_offset;
  uint32_t old_length;
  uint32_t new_offset;
  uint32_t new_length;
  uint32_t exe_type;  // ExecutableType.
  uint16_t version = kInvalidVersion;
};

// Sanity check.
static_assert(sizeof(PatchElementHeader) == 22,
              "PatchElementHeader must be 22 bytes");

#pragma pack(pop)

// Descibes a raw FIX operation.
struct RawDeltaUnit {
  offset_t copy_offset;  // Offset in copy regions.
  int8_t diff;           // Bytewise difference.
};

// A Zucchini patch contains data streams encoded using varint format to reduce
// uncompressed size.

// Writes |value| as a varint in |dst| and returns an iterator pointing beyond
// the written region. |dst| is assumed to hold enough space. Typically, this
// will write to a vector using back insertion, e.g.:
//   EncodeVarUInt(value, std::back_inserter(vector));
template <class T, class It>
It EncodeVarUInt(T value, It dst) {
  static_assert(std::is_unsigned<T>::value, "Value type must be unsigned");

  while (value >= 0x80) {
    *dst++ = static_cast<uint8_t>(value) | 0x80;
    value >>= 7;
  }
  *dst++ = static_cast<uint8_t>(value);
  return dst;
}

// Same as EncodeVarUInt(), but for signed values.
template <class T, class It>
It EncodeVarInt(T value, It dst) {
  static_assert(std::is_signed<T>::value, "Value type must be signed");

  using unsigned_value_type = typename std::make_unsigned<T>::type;
  if (value < 0)
    return EncodeVarUInt((unsigned_value_type(~value) << 1) | 1, dst);
  else
    return EncodeVarUInt(unsigned_value_type(value) << 1, dst);
}

// Tries to read a varint unsigned integer from |[first, last)|. If
// succesful, writes result into |value| and returns the number of bytes
// read from |[first, last)|. Otherwise returns 0.
template <class T, class It>
typename std::iterator_traits<It>::difference_type DecodeVarUInt(It first,
                                                                 It last,
                                                                 T* value) {
  static_assert(std::is_unsigned<T>::value, "Value type must be unsigned");

  uint8_t sh = 0;
  T val = 0;
  for (auto it = first; it != last;) {
    val |= T(*it & 0x7F) << sh;
    if (*(it++) < 0x80) {
      *value = val;
      return it - first;
    }
    sh += 7;
    if (sh >= sizeof(T) * 8)  // Overflow!
      return 0;
  }
  return 0;
}

// Same as DecodeVarUInt(), but for signed values.
template <class T, class It>
typename std::iterator_traits<It>::difference_type DecodeVarInt(It first,
                                                                It last,
                                                                T* value) {
  static_assert(std::is_signed<T>::value, "Value type must be signed");

  typename std::make_unsigned<T>::type tmp = 0;
  auto res = DecodeVarUInt(first, last, &tmp);
  if (res) {
    if (tmp & 1)
      *value = ~static_cast<T>(tmp >> 1);
    else
      *value = static_cast<T>(tmp >> 1);
  }
  return res;
}

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_PATCH_UTILS_H_
