// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/han_kerning.h"

#include <unicode/uchar.h>

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_features.h"
#include "third_party/blink/renderer/platform/fonts/shaping/font_features.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {

// Get `CharType` from the glyph bounding box.
HanKerning::CharType GetType(const SkRect& bound,
                             float em,
                             bool is_horizontal) {
  const float half_em = em / 2;
  if (is_horizontal) {
    if (bound.right() <= half_em) {
      return HanKerning::CharType::kClose;
    }
    if (bound.width() <= half_em && bound.left() >= em / 4) {
      return HanKerning::CharType::kMiddle;
    }
  } else {
    if (bound.bottom() <= half_em) {
      return HanKerning::CharType::kClose;
    }
    if (bound.height() <= half_em && bound.top() >= em / 4) {
      return HanKerning::CharType::kMiddle;
    }
  }
  return HanKerning::CharType::kOther;
}

HanKerning::CharType GetType(base::span<SkRect> bounds,
                             float em,
                             bool is_horizontal) {
  const HanKerning::CharType type0 = GetType(bounds.front(), em, is_horizontal);
  // To simplify the logic, all types must be the same, or don't apply kerning.
  for (const SkRect bound : bounds.subspan(1)) {
    const HanKerning::CharType type = GetType(bound, em, is_horizontal);
    if (type != type0) {
      return HanKerning::CharType::kOther;
    }
  }
  return type0;
}

}  // namespace

// Compute the character class.
// See Text Spacing Character Classes:
// https://drafts.csswg.org/css-text-4/#text-spacing-classes
HanKerning::CharType HanKerning::GetCharType(UChar ch,
                                             const FontData& font_data) {
  // TODO(crbug.com/1463890): This logic is only for prototyping.
  switch (ch) {
    case kIdeographicCommaCharacter:     // U+3001
    case kIdeographicFullStopCharacter:  // U+3002
    case kFullwidthComma:                // U+FF0C
    case kFullwidthFullStop:             // U+FF0E
      return font_data.type_for_dot;
    case kFullwidthColon:      // U+FF1A
      return font_data.type_for_colon;
    case kFullwidthSemicolon:  // U+FF1B
      return font_data.type_for_semicolon;
    case kLeftSingleQuotationMarkCharacter:  // U+2018
    case kLeftDoubleQuotationMarkCharacter:  // U+201C
      return CharType::kOpen;
    case kRightSingleQuotationMarkCharacter:  // U+2019
    case kRightDoubleQuotationMarkCharacter:  // U+201D
      return CharType::kClose;
    case kIdeographicSpaceCharacter:  // U+3000
    case kKatakanaMiddleDot:          // U+30FB
      return CharType::kMiddle;
  }
  if (Character::IsBlockCjkSymbolsAndPunctuation(ch) ||
      Character::IsEastAsianWidthFullwidth(ch)) {
    const auto gc = static_cast<UCharCategory>(u_charType(ch));
    if (gc == UCharCategory::U_START_PUNCTUATION) {
      return CharType::kOpen;
    }
    if (gc == UCharCategory::U_END_PUNCTUATION) {
      return CharType::kClose;
    }
  }
  return CharType::kOther;
}

inline bool HanKerning::ShouldKern(CharType type, CharType last_type) {
  return type == CharType::kOpen &&
         (last_type == CharType::kOpen || last_type == CharType::kMiddle ||
          last_type == CharType::kClose);
}

inline bool HanKerning::ShouldKernLast(CharType type, CharType last_type) {
  return last_type == CharType::kClose &&
         (type == CharType::kClose || type == CharType::kMiddle);
}

// Compute kerning and apply features.
// See Fullwidth Punctuation Collapsing:
// https://drafts.csswg.org/css-text-4/#fullwidth-collapsing
void HanKerning::Compute(const String& text,
                         wtf_size_t start,
                         wtf_size_t end,
                         const SimpleFontData& font,
                         const FontDescription& font_description,
                         bool is_horizontal,
                         FontFeatures& features) {
  if (UNLIKELY(font_description.GetTextSpacingTrim() !=
               TextSpacingTrim::kSpaceFirst)) {
    return;
  }
  const LayoutLocale& locale = font_description.LocaleOrDefault();
  const FontData& font_data = font.HanKerningData(locale, is_horizontal);
  if (!font_data.has_alternate_spacing) {
    return;
  }

  // Compute for the first character.
  Vector<wtf_size_t, 32> indices;
  CharType last_type;
  if (start) {
    last_type = GetCharType(text[start - 1], font_data);
    const CharType type = GetCharType(text[start], font_data);
    if (ShouldKern(type, last_type)) {
      indices.push_back(start);
    }
    last_type = type;
  } else {
    last_type = GetCharType(text[start], font_data);
  }

  if (font_data.has_contextual_spacing) {
    // The `chws` feature can handle charcters in a run.
    // Compute the end edge if there are following runs.
    if (end < text.length()) {
      if (end - 1 > start) {
        last_type = GetCharType(text[end - 1], font_data);
      }
      const CharType type = GetCharType(text[end], font_data);
      if (ShouldKernLast(type, last_type)) {
        indices.push_back(end - 1);
      }
    }
  } else {
    // Compute for characters in the middle.
    CharType type;
    for (wtf_size_t i = start + 1; i < end; ++i, last_type = type) {
      const UChar ch = text[i];
      type = GetCharType(ch, font_data);
      if (ShouldKernLast(type, last_type)) {
        DCHECK_GT(i, 0u);
        indices.push_back(i - 1);
      } else if (ShouldKern(type, last_type)) {
        indices.push_back(i);
      }
    }

    // Compute for the last character.
    if (end < text.length()) {
      type = GetCharType(text[end], font_data);
      if (ShouldKernLast(type, last_type)) {
        indices.push_back(end - 1);
      }
    }
  }

  // Append to `features`.
  if (indices.empty()) {
    return;
  }
  DCHECK(std::is_sorted(indices.begin(), indices.end(), std::less_equal<>()));
  const hb_tag_t tag =
      is_horizontal ? HB_TAG('h', 'a', 'l', 't') : HB_TAG('v', 'h', 'a', 'l');
  features.Reserve(features.size() + indices.size());
  for (const wtf_size_t i : indices) {
    features.Append({tag, 1, i, i + 1});
  }
}

HanKerning::FontData::FontData(const SimpleFontData& font,
                               const LayoutLocale& locale,
                               bool is_horizontal) {
  // Check if the font has `halt` (or `vhal` in vertical.)
  OpenTypeFeatures features(font);
  const hb_tag_t alt_tag =
      is_horizontal ? HB_TAG('h', 'a', 'l', 't') : HB_TAG('v', 'h', 'a', 'l');
  has_alternate_spacing = features.Contains(alt_tag);
  if (!has_alternate_spacing) {
    return;
  }

  // Check if the font has `chws` (or `vchw` in vertical.)
  const hb_tag_t chws_tag =
      is_horizontal ? HB_TAG('c', 'h', 'w', 's') : HB_TAG('v', 'c', 'h', 'w');
  has_contextual_spacing = features.Contains(chws_tag);

  // Some code points change their glyphs by languages, and it may change
  // `CharType` that depends on glyphs bounds as well.
  // https://drafts.csswg.org/css-text-4/#text-spacing-classes
  //
  // For example, the Adobe's common convention is to:
  // * Place full stop and comma at center only for Traditional Chinese.
  // * Place colon and semicolon on the left only for Simplified Chinese.
  // https://github.com/adobe-fonts/source-han-sans/raw/release/SourceHanSansReadMe.pdf
  const UChar kChars[] = {
      // Dot (full stop and comma) characters.
      // https://drafts.csswg.org/css-text-4/#fullwidth-dot-punctuation
      kIdeographicCommaCharacter, kIdeographicFullStopCharacter,
      kFullwidthComma, kFullwidthFullStop,
      // Colon characters.
      // https://drafts.csswg.org/css-text-4/#fullwidth-colon-punctuation
      kFullwidthColon, kFullwidthSemicolon};
  constexpr unsigned kDotStartIndex = 0;
  constexpr unsigned kDotSize = 4;
  constexpr unsigned kColonIndex = 4;
  constexpr unsigned kSemicolonIndex = 5;
  static_assert(kDotStartIndex + kDotSize <= std::size(kChars));
  static_assert(kColonIndex < std::size(kChars));
  static_assert(kSemicolonIndex < std::size(kChars));

  // Use `HarfBuzzShaper` to find the correct glyph ID.
  //
  // The glyph changes are often done by different encodings (`cmap`) or by
  // OpenType features such as `calt`. In vertical flow, some glyphs change,
  // which is done by OpenType features such as `vert`. Shaping is needed to
  // apply these features.
  HarfBuzzShaper shaper(String(kChars, std::size(kChars)));
  HarfBuzzShaper::GlyphDataList glyph_data_list;
  shaper.GetGlyphData(font, locale, locale.GetScriptForHan(), is_horizontal,
                      glyph_data_list);

  // All characters must meet the following conditions:
  // * Has one glyph for one character.
  // * Its advance is 1ch.
  // Also prepare `glyphs` for `BoundsForGlyphs` while checking.
  if (glyph_data_list.size() != std::size(kChars)) {
    has_alternate_spacing = false;
    return;
  }
  Vector<Glyph, 256> glyphs;
  const float em = font.GetFontMetrics().IdeographicFullWidth().value_or(
      font.PlatformData().size());
  unsigned cluster = 0;
  for (const HarfBuzzShaper::GlyphData& glyph_data : glyph_data_list) {
    if (!glyph_data.glyph || glyph_data.cluster != cluster ||
        (is_horizontal ? glyph_data.advance.x() : glyph_data.advance.y()) !=
            em) {
      has_alternate_spacing = false;
      return;
    }
    glyphs.push_back(glyph_data.glyph);
    ++cluster;
  }
  DCHECK_EQ(glyphs.size(), std::size(kChars));

  // Compute glyph bounds for all glyphs.
  Vector<SkRect, 256> bounds(glyphs.size());
  font.BoundsForGlyphs(glyphs, &bounds);
  // `bounds` are relative to the glyph origin. Adjust them to be relative to
  // the paint origin.
  DCHECK_EQ(glyph_data_list.size(), bounds.size());
  for (wtf_size_t i = 0; i < glyph_data_list.size(); ++i) {
    const HarfBuzzShaper::GlyphData& glyph_data = glyph_data_list[i];
    bounds[i].offset({glyph_data.offset.x(), glyph_data.offset.y()});
  }

  // Compute types from glyph bounds.
  DCHECK_EQ(bounds.size(), std::size(kChars));
  type_for_dot = GetType(base::make_span(bounds.begin() + kDotStartIndex,
                                         kDotStartIndex + kDotSize),
                         em, is_horizontal);
  type_for_colon = GetType(bounds[kColonIndex], em, is_horizontal);
  type_for_semicolon = GetType(bounds[kSemicolonIndex], em, is_horizontal);
}

}  // namespace blink
