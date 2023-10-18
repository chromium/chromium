// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_PROPERTY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_PROPERTY_H_

#include <cstdint>

namespace blink {

using CharacterPropertyType = uint8_t;

enum class CharacterProperty : CharacterPropertyType {
  kIsCJKIdeographOrSymbol = 1 << 0,
  kIsPotentialCustomElementNameChar = 1 << 1,
  kIsBidiControl = 1 << 2,
  kIsHangul = 1 << 3,

  // Bits to store `HanKerningCharType`.
  kHanKerningShift = 4,
  kHanKerningSize = 4,
  kHanKerningMask = ((1 << kHanKerningSize) - 1),
  kHanKerningShiftedMask = kHanKerningMask << kHanKerningShift,

  kNumBits = kHanKerningShift + kHanKerningSize,
};
static_assert(static_cast<unsigned>(CharacterProperty::kNumBits) <=
              sizeof(CharacterPropertyType) * 8);

inline CharacterProperty operator|(CharacterProperty a, CharacterProperty b) {
  return static_cast<CharacterProperty>(static_cast<CharacterPropertyType>(a) |
                                        static_cast<CharacterPropertyType>(b));
}

inline CharacterProperty operator&(CharacterProperty a, CharacterProperty b) {
  return static_cast<CharacterProperty>(static_cast<CharacterPropertyType>(a) &
                                        static_cast<CharacterPropertyType>(b));
}

inline CharacterProperty operator|=(CharacterProperty& a, CharacterProperty b) {
  a = a | b;
  return a;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_PROPERTY_H_
