/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_ASCII_FAST_PATH_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_ASCII_FAST_PATH_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"

#if defined(OS_MAC) && defined(ARCH_CPU_X86_FAMILY)
#include <emmintrin.h>
#endif

namespace WTF {

// Assuming that a pointer is the size of a "machine word", then
// uintptr_t is an integer type that is also a machine word.
typedef uintptr_t MachineWord;
const uintptr_t kMachineWordAlignmentMask = sizeof(MachineWord) - 1;

inline bool IsAlignedToMachineWord(const void* pointer) {
  return !(reinterpret_cast<uintptr_t>(pointer) & kMachineWordAlignmentMask);
}

template <typename T>
inline T* AlignToMachineWord(T* pointer) {
  return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(pointer) &
                              ~kMachineWordAlignmentMask);
}

template <size_t size, typename CharacterType>
struct NonASCIIMask;
template <>
struct NonASCIIMask<4, UChar> {
  static inline uint32_t Value() { return 0xFF80FF80U; }
};
template <>
struct NonASCIIMask<4, LChar> {
  static inline uint32_t Value() { return 0x80808080U; }
};
template <>
struct NonASCIIMask<8, UChar> {
  static inline uint64_t Value() { return 0xFF80FF80FF80FF80ULL; }
};
template <>
struct NonASCIIMask<8, LChar> {
  static inline uint64_t Value() { return 0x8080808080808080ULL; }
};

template <typename CharacterType>
inline bool IsAllASCII(MachineWord word) {
  return !(word & NonASCIIMask<sizeof(MachineWord), CharacterType>::Value());
}

struct ASCIIStringAttributes {
  ASCIIStringAttributes(bool contains_only_ascii, bool is_lower_ascii)
      : contains_only_ascii(contains_only_ascii),
        is_lower_ascii(is_lower_ascii) {}
  unsigned contains_only_ascii : 1;
  unsigned is_lower_ascii : 1;
};

// Note: This function assumes the input is likely all ASCII, and
// does not leave early if it is not the case.
template <typename CharacterType>
ALWAYS_INLINE ASCIIStringAttributes
CharacterAttributes(const CharacterType* characters, size_t length) {
  DCHECK_GT(length, 0u);

  // Performance note: This loop will not vectorize properly in -Oz. Ensure
  // the calling code is built with -O2.
  CharacterType all_char_bits = 0;
  bool contains_upper_case = false;
  for (size_t i = 0; i < length; i++) {
    all_char_bits |= characters[i];
    contains_upper_case |= IsASCIIUpper(characters[i]);
  }

  return ASCIIStringAttributes(IsASCII(all_char_bits), !contains_upper_case);
}

template <typename CharacterType>
ALWAYS_INLINE bool IsLowerASCII(const CharacterType* characters,
                                size_t length) {
  bool contains_upper_case = false;
  for (wtf_size_t i = 0; i < length; i++) {
    contains_upper_case |= IsASCIIUpper(characters[i]);
  }
  return !contains_upper_case;
}

template <typename CharacterType>
ALWAYS_INLINE bool IsUpperASCII(const CharacterType* characters,
                                size_t length) {
  bool contains_lower_case = false;
  for (wtf_size_t i = 0; i < length; i++) {
    contains_lower_case |= IsASCIILower(characters[i]);
  }
  return !contains_lower_case;
}

class LowerConverter {
 public:
  template <typename CharType>
  ALWAYS_INLINE static bool IsCorrectCase(CharType* characters, size_t length) {
    return IsLowerASCII(characters, length);
  }

  template <typename CharType>
  ALWAYS_INLINE static CharType Convert(CharType ch) {
    return ToASCIILower(ch);
  }
};

class UpperConverter {
 public:
  template <typename CharType>
  ALWAYS_INLINE static bool IsCorrectCase(CharType* characters, size_t length) {
    return IsUpperASCII(characters, length);
  }

  template <typename CharType>
  ALWAYS_INLINE static CharType Convert(CharType ch) {
    return ToASCIIUpper(ch);
  }
};

template <typename StringType, typename Converter, typename Allocator>
ALWAYS_INLINE typename Allocator::ResultStringType ConvertASCIICase(
    const StringType& string,
    Converter&& converter,
    Allocator&& allocator) {
  CHECK_LE(string.length(), std::numeric_limits<wtf_size_t>::max());

  // First scan the string for uppercase and non-ASCII characters:
  wtf_size_t length = string.length();
  if (string.Is8Bit()) {
    if (converter.IsCorrectCase(string.Characters8(), length)) {
      return allocator.CoerceOriginal(string);
    }

    LChar* data8;
    auto new_impl = allocator.Alloc(length, data8);

    for (wtf_size_t i = 0; i < length; ++i) {
      data8[i] = converter.Convert(string.Characters8()[i]);
    }
    return new_impl;
  }

  if (converter.IsCorrectCase(string.Characters16(), length)) {
    return allocator.CoerceOriginal(string);
  }

  UChar* data16;
  auto new_impl = allocator.Alloc(length, data16);

  for (wtf_size_t i = 0; i < length; ++i) {
    data16[i] = converter.Convert(string.Characters16()[i]);
  }
  return new_impl;
}

inline void CopyLCharsFromUCharSource(LChar* destination,
                                      const UChar* source,
                                      size_t length) {
#if defined(OS_MAC) && defined(ARCH_CPU_X86_FAMILY)
  const uintptr_t kMemoryAccessSize =
      16;  // Memory accesses on 16 byte (128 bit) alignment
  const uintptr_t kMemoryAccessMask = kMemoryAccessSize - 1;

  size_t i = 0;
  for (; i < length &&
         reinterpret_cast<uintptr_t>(&source[i]) & kMemoryAccessMask;
       ++i) {
    DCHECK(!(source[i] & 0xff00));
    destination[i] = static_cast<LChar>(source[i]);
  }

  const uintptr_t kSourceLoadSize =
      32;  // Process 32 bytes (16 UChars) each iteration
  const size_t kUcharsPerLoop = kSourceLoadSize / sizeof(UChar);
  if (length > kUcharsPerLoop) {
    const size_t end_length = length - kUcharsPerLoop + 1;
    for (; i < end_length; i += kUcharsPerLoop) {
#if DCHECK_IS_ON()
      for (unsigned check_index = 0; check_index < kUcharsPerLoop;
           ++check_index)
        DCHECK(!(source[i + check_index] & 0xff00));
#endif
      __m128i first8u_chars =
          _mm_load_si128(reinterpret_cast<const __m128i*>(&source[i]));
      __m128i second8u_chars =
          _mm_load_si128(reinterpret_cast<const __m128i*>(&source[i + 8]));
      __m128i packed_chars = _mm_packus_epi16(first8u_chars, second8u_chars);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(&destination[i]),
                       packed_chars);
    }
  }

  for (; i < length; ++i) {
    DCHECK(!(source[i] & 0xff00));
    destination[i] = static_cast<LChar>(source[i]);
  }
#elif defined(COMPILER_GCC) && defined(CPU_ARM_NEON) && \
    !defined(ARCH_CPU_BIG_ENDIAN) && defined(NDEBUG)
  const LChar* const end = destination + length;
  const uintptr_t kMemoryAccessSize = 8;

  if (length >= (2 * kMemoryAccessSize) - 1) {
    // Prefix: align dst on 64 bits.
    const uintptr_t kMemoryAccessMask = kMemoryAccessSize - 1;
    while (reinterpret_cast<uintptr_t>(destination) & kMemoryAccessMask)
      *destination++ = static_cast<LChar>(*source++);

    // Vector interleaved unpack, we only store the lower 8 bits.
    const uintptr_t length_left = end - destination;
    const LChar* const simd_end = end - (length_left % kMemoryAccessSize);
    do {
      asm("vld2.8   { d0-d1 }, [%[SOURCE]] !\n\t"
          "vst1.8   { d0 }, [%[DESTINATION],:64] !\n\t"
          : [SOURCE] "+r"(source), [DESTINATION] "+r"(destination)
          :
          : "memory", "d0", "d1");
    } while (destination != simd_end);
  }

  while (destination != end)
    *destination++ = static_cast<LChar>(*source++);
#else
  for (size_t i = 0; i < length; ++i) {
    DCHECK(!(source[i] & 0xff00));
    destination[i] = static_cast<LChar>(source[i]);
  }
#endif
}

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_ASCII_FAST_PATH_H_
