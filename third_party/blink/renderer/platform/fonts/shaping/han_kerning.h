// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HAN_KERNING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HAN_KERNING_H_

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class FontDescription;
class FontFeatures;
class LayoutLocale;
class SimpleFontData;

//
// This class implements the behavior necessary for the CSS `text-spacing-trim`
// property[1].
//
// In short, it's similar to kerning to be applied to Han-derived scripts such
// as Chinese or Japanese. In OpenType, this behavior is defined as the
// `chws`[2] feature.
//
// It's different from the regular kerning that the kerning pairs and amounts
// are computable. There are tools to add the features to existing fonts[3][4].
//
// This class complements the OpenType feature in that:
// 1. Handles the desired beahvior at the font boundaries. OpenType features
//    can't handle kerning at font boundaries by design.
// 2. Emulates the behavior when the font doesn't have the `chws` feature.
//
// [1]: https://drafts.csswg.org/css-text-4/#text-spacing-trim-property
// [2]:
// https://learn.microsoft.com/en-us/typography/opentype/spec/features_ae#tag-chws
// [3]: https://github.com/googlefonts/chws_tool
// [4]: https://github.com/kojiishi/east_asian_spacing
//
class PLATFORM_EXPORT HanKerning {
  STACK_ALLOCATED();

 public:
  HanKerning(const String& text,
             wtf_size_t start,
             wtf_size_t end,
             const SimpleFontData& font_data,
             const FontDescription& font_description,
             bool is_horizontal,
             FontFeatures& features) {
    if (!RuntimeEnabledFeatures::CSSTextSpacingTrimEnabled()) {
      return;
    }
    // TODO(crbug.com/1463890): Add more conditions to fail fast.
    Compute(text, start, end, font_data, font_description, is_horizontal,
            features);
  }

  enum class CharType : uint8_t {
    kOther,
    kOpen,
    kClose,
    kMiddle,
  };

  // Data retrieved from fonts for `HanKerning`.
  struct PLATFORM_EXPORT FontData {
    FontData() = default;
    FontData(const SimpleFontData& font,
             const LayoutLocale& locale,
             bool is_horizontal);

    // True if this font has `halt` (or `vhal` in vertical.)
    // https://learn.microsoft.com/en-us/typography/opentype/spec/features_fj#tag-halt
    bool has_alternate_spacing = false;
    // True if this font has `chws` (or `vchw` in vertical.)
    // https://learn.microsoft.com/en-us/typography/opentype/spec/features_ae#tag-chws
    bool has_contextual_spacing = false;

    // `CharType` for "fullwidth dot punctuation."
    // https://drafts.csswg.org/css-text-4/#text-spacing-classes
    CharType type_for_dot = CharType::kOther;
    // `CharType` for "fullwidth colon punctuation."
    // Type for colon and semicolon are separated, to support the Adobe's
    // convention for vertical flow, which rotates Japanese colon, but doesn't
    // rotate semicolon.
    CharType type_for_colon = CharType::kOther;
    CharType type_for_semicolon = CharType::kOther;
  };

 private:
  static CharType GetCharType(UChar ch, const FontData& font_data);

  static bool ShouldKern(CharType type, CharType last_type);
  static bool ShouldKernLast(CharType type, CharType last_type);

  static void Compute(const String& text,
                      wtf_size_t start,
                      wtf_size_t end,
                      const SimpleFontData& font_data,
                      const FontDescription& font_description,
                      bool is_horizontal,
                      FontFeatures& features);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HAN_KERNING_H_
