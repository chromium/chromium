// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_PROPERTY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_PROPERTY_H_

#include <cstdint>

namespace blink {

using CharacterPropertyType = uint8_t;

enum class CharacterProperty : CharacterPropertyType {
  kIsCJKIdeographOrSymbol = 0x0001,
  kIsUprightInMixedVertical = 0x0002,
  kIsPotentialCustomElementNameChar = 0x0004,
  kIsBidiControl = 0x0008,
  kIsHangul = 0x0010
};

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
