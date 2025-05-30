// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_MAC_CHARACTER_FALLBACK_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_MAC_CHARACTER_FALLBACK_CACHE_H_

#include <optional>

#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

struct PLATFORM_EXPORT CharacterFallbackKey {
  String font_identifier = "";
  int16_t weight = 0;
  int16_t style = 0;
  float font_size = 0.f;
  uint8_t orientation = 0;
  bool operator==(const CharacterFallbackKey&) const = default;
  static std::optional<CharacterFallbackKey> Make(CTFontRef ct_font,
                                                  int16_t raw_font_weight,
                                                  int16_t raw_font_style,
                                                  uint8_t orientation,
                                                  float font_size);
};

struct PLATFORM_EXPORT CharacterFallbackKeyHashTraits
    : GenericHashTraits<CharacterFallbackKey> {
  STATIC_ONLY(CharacterFallbackKeyHashTraits);

  static unsigned GetHash(const CharacterFallbackKey& key);

  static bool Equal(const CharacterFallbackKey& a,
                    const CharacterFallbackKey& b) {
    return a == b;
  }

  static CharacterFallbackKey EmptyValue() {
    CharacterFallbackKey empty_value;
    empty_value.font_size = -1.0;
    return empty_value;
  }

  static CharacterFallbackKey DeletedValue() {
    CharacterFallbackKey deleted_value;
    deleted_value.font_size = -2.0;
    return deleted_value;
  }

  static constexpr bool kSafeToCompareToEmptyOrDeleted = true;
};

using CharacterFallbackCache = HeapHashMap<Member<const CharacterFallbackKey>,
                                           WeakMember<const SimpleFontData>,
                                           CharacterFallbackKeyHashTraits>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_MAC_CHARACTER_FALLBACK_CACHE_H_
