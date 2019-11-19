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
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

#if defined(OS_MACOSX) && defined(ARCH_CPU_X86_FAMILY)
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

// Note: This function assume the input is likely all ASCII, and
// does not leave early if it is not the case.
template <typename CharacterType>
inline bool CharactersAreAllASCII(const CharacterType* characters,
                                  size_t length) {
  DCHECK_GT(length, 0u);
  MachineWord all_char_bits = 0;
  const CharacterType* end = characters + length;

  // Prologue: align the input.
  while (!IsAlignedToMachineWord(characters) && characters != end) {
    all_char_bits |= *characters;
    ++characters;
  }

  // Compare the values of CPU word size.
  const CharacterType* word_end = AlignToMachineWord(end);
  const size_t kLoopIncrement = sizeof(MachineWord) / sizeof(CharacterType);
  while (characters < word_end) {
    all_char_bits |= *(reinterpret_cast_ptr<const MachineWord*>(characters));
    characters += kLoopIncrement;
  }

  // Process the remaining bytes.
  while (characters != end) {
    all_char_bits |= *characters;
    ++characters;
  }

  MachineWord non_ascii_bit_mask =
      NonASCIIMask<sizeof(MachineWord), CharacterType>::Value();
  return !(all_char_bits & non_ascii_bit_mask);
}

inline void CopyLCharsFromUCharSource(LChar* destination,
                                      const UChar* source,
                                      size_t length) {
#if defined(OS_MACOSX) && defined(ARCH_CPU_X86_FAMILY)
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
