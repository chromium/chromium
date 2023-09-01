// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_EAST_ASIAN_SPACING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_EAST_ASIAN_SPACING_H_

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class FontFeatures;
class SimpleFontData;

//
// This class implements the behavior necessary for the CSS `text-spacing-trim`
// property[1].
//
// The OpenType `chws`[1] feature is designed to implement the CSS property,
// but this class complements it in that:
// 1. Handles the desired beahvior at the font boundaries. OpenType features
//    can't handle kerning at font boundaries by design.
// 2. Emulates the behavior when the font doesn't have the `chws` feature.
//
// [1]: https://drafts.csswg.org/css-text-4/#text-spacing-trim-property
// [2]:
// https://learn.microsoft.com/en-us/typography/opentype/spec/features_ae#tag-chws
//
class EastAsianSpacing {
  STACK_ALLOCATED();

 public:
  EastAsianSpacing(const String& text,
                   wtf_size_t start,
                   wtf_size_t end,
                   const SimpleFontData& font_data,
                   FontFeatures& features) {
    if (!RuntimeEnabledFeatures::CSSTextSpacingTrimEnabled()) {
      return;
    }
    // TODO(crbug.com/1463890): Add more conditions to fail fast.
    ComputeKerning(text, start, end, font_data, features);
  }

 private:
  enum class CharType {
    Other,
    Open,
    Close,
    Middle,
  };

  static CharType GetCharType(UChar ch);

  static void ComputeKerning(const String& text,
                             wtf_size_t start,
                             wtf_size_t end,
                             const SimpleFontData& font_data,
                             FontFeatures& features);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_EAST_ASIAN_SPACING_H_
