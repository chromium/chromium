// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_PROPERTY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_PROPERTY_H_

#include <cstdint>

#include "third_party/blink/renderer/platform/text/east_asian_spacing_type.h"
#include "third_party/blink/renderer/platform/text/han_kerning_char_type.h"

namespace blink {

using CharacterPropertyType = uint16_t;

struct CharacterProperty {
  CharacterProperty() : CharacterProperty(0) {}

  explicit CharacterProperty(CharacterPropertyType value) {
    static_assert(sizeof(CharacterProperty) == sizeof(CharacterPropertyType));
    *reinterpret_cast<CharacterPropertyType*>(this) = value;
  }

  CharacterPropertyType AsUnsigned() const {
    static_assert(sizeof(CharacterProperty) == sizeof(CharacterPropertyType));
    return *reinterpret_cast<const CharacterPropertyType*>(this);
  }

  bool operator==(const CharacterProperty& other) const {
    return AsUnsigned() == other.AsUnsigned();
  }

  bool is_cjk_ideograph_or_symbol : 1;
  bool is_potential_custom_element_name_char : 1;
  bool is_bidi_control : 1;
  bool is_hangul : 1;
  HanKerningCharType han_kerning : 4;
  EastAsianSpacingType east_asian_spacing : 2;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TEXT_CHARACTER_PROPERTY_H_
