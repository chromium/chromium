// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/east_asian_spacing.h"

#include <unicode/uchar.h>

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_features.h"
#include "third_party/blink/renderer/platform/fonts/shaping/font_features.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {

inline bool IsCjkSymbolsAndPunctuationOrEastAsianFullwidth(UChar ch) {
  return (ch >= 0x3000 && ch <= 0x303F) ||
         static_cast<UEastAsianWidth>(u_getIntPropertyValue(
             ch, UCHAR_EAST_ASIAN_WIDTH)) == UEastAsianWidth::U_EA_FULLWIDTH;
}

// Get `CharType` from the glyph bounding box.
EastAsianSpacing::CharType GetType(const SkRect& bound,
                                   float em,
                                   bool is_horizontal) {
  const float half_em = em / 2;
  if (is_horizontal) {
    if (bound.right() <= half_em) {
      return EastAsianSpacing::CharType::kClose;
    }
    if (bound.width() <= half_em && bound.left() >= em / 4) {
      return EastAsianSpacing::CharType::kMiddle;
    }
  } else {
    if (bound.bottom() <= half_em) {
      return EastAsianSpacing::CharType::kClose;
    }
    if (bound.height() <= half_em && bound.top() >= em / 4) {
      return EastAsianSpacing::CharType::kMiddle;
    }
  }
  return EastAsianSpacing::CharType::kOther;
}

EastAsianSpacing::CharType GetType(base::span<SkRect> bounds,
                                   float em,
                                   bool is_horizontal) {
  const EastAsianSpacing::CharType type0 =
      GetType(bounds.front(), em, is_horizontal);
  // To simplify the logic, all types must be the same, or don't apply kerning.
  for (const SkRect bound : bounds.subspan(1)) {
    const EastAsianSpacing::CharType type = GetType(bound, em, is_horizontal);
    if (type != type0) {
      return EastAsianSpacing::CharType::kOther;
    }
  }
  return type0;
}

}  // namespace

// Compute the character class.
// See Text Spacing Character Classes:
// https://drafts.csswg.org/css-text-4/#text-spacing-classes
EastAsianSpacing::CharType EastAsianSpacing::GetCharType(
    UChar ch,
    const FontData& font_data) {
  // TODO(crbug.com/1463890): This logic is only for prototyping.
  switch (ch) {
    case kIdeographicCommaCharacter:     // U+3001
    case kIdeographicFullStopCharacter:  // U+3002
    case kFullwidthComma:                // U+FF0C
    case kFullwidthFullStop:             // U+FF0E
      return font_data.type_for_dot;
    case kFullwidthColon:      // U+FF1A
    case kFullwidthSemicolon:  // U+FF1B
      return font_data.type_for_colon;
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
  const auto gc = static_cast<UCharCategory>(u_charType(ch));
  if (gc == UCharCategory::U_START_PUNCTUATION &&
      IsCjkSymbolsAndPunctuationOrEastAsianFullwidth(ch)) {
    return CharType::kOpen;
  }
  if (gc == UCharCategory::U_END_PUNCTUATION &&
      IsCjkSymbolsAndPunctuationOrEastAsianFullwidth(ch)) {
    return CharType::kClose;
  }
  return CharType::kOther;
}

inline bool EastAsianSpacing::ShouldKern(CharType type, CharType last_type) {
  return type == CharType::kOpen &&
         (last_type == CharType::kOpen || last_type == CharType::kMiddle ||
          last_type == CharType::kClose);
}

inline bool EastAsianSpacing::ShouldKernLast(CharType type,
                                             CharType last_type) {
  return last_type == CharType::kClose &&
         (type == CharType::kClose || type == CharType::kMiddle);
}

// Compute kerning and apply features.
// See Fullwidth Punctuation Collapsing:
// https://drafts.csswg.org/css-text-4/#fullwidth-collapsing
void EastAsianSpacing::ComputeKerning(const String& text,
                                      wtf_size_t start,
                                      wtf_size_t end,
                                      const SimpleFontData& font,
                                      bool is_horizontal,
                                      FontFeatures& features) {
  // TODO(crbug.com/1463890): Cache `FontData`.
  FontData font_data(font, is_horizontal);
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

  // Compute for characters in the middle.
  // TODO(crbug.com/1463891): This part can be skipped if the font has `chws`.
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

EastAsianSpacing::FontData::FontData(const SimpleFontData& font,
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

  // Some characters change their `CharType` depends on where glyphs are in the
  // glyph space. Check glyph bounding box.
  // https://drafts.csswg.org/css-text-4/#text-spacing-classes
  const UChar32 chars[] = {kIdeographicCommaCharacter,
                           kIdeographicFullStopCharacter,
                           kFullwidthComma,
                           kFullwidthFullStop,
                           kFullwidthColon,
                           kFullwidthSemicolon};
  Vector<Glyph, 256> glyphs;
  for (const UChar32 ch : chars) {
    glyphs.push_back(font.GlyphForCharacter(ch));
  }
  Vector<SkRect, 256> bounds(glyphs.size());
  font.BoundsForGlyphs(glyphs, &bounds);
  const float em = font.GetFontMetrics().IdeographicFullWidth().value_or(
      font.PlatformData().size());
  type_for_dot =
      GetType(base::make_span(bounds.begin(), 4u), em, is_horizontal);
  type_for_colon =
      GetType(base::make_span(bounds.begin() + 4, 2u), em, is_horizontal);
}

}  // namespace blink
