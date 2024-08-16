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

HashSet<uint32_t> ExclusiveFeatures() {
  // https://learn.microsoft.com/en-us/typography/opentype/spec/features_ae#chws
  // https://learn.microsoft.com/en-us/typography/opentype/spec/features_uz#vchw
  return HashSet<uint32_t>{
      HB_TAG('h', 'a', 'l', 't'), HB_TAG('h', 'w', 'i', 'd'),
      HB_TAG('p', 'a', 'l', 't'), HB_TAG('p', 'w', 'i', 'd'),
      HB_TAG('q', 'w', 'i', 'd'), HB_TAG('t', 'w', 'i', 'd'),
      HB_TAG('v', 'a', 'l', 't'), HB_TAG('v', 'h', 'a', 'l'),
      HB_TAG('v', 'p', 'a', 'l'),
  };
}

bool IsExclusiveFeature(uint32_t tag) {
  DEFINE_STATIC_LOCAL(HashSet<uint32_t>, tags, (ExclusiveFeatures()));
  return tags.Contains(tag);
}

inline float GetAdvance(const HarfBuzzShaper::GlyphData& glyph,
                        bool is_horizontal) {
  return is_horizontal ? glyph.advance.x() : glyph.advance.y();
  ;
}

// Compute `CharType` from the glyph bounding box.
HanKerning::CharType CharTypeFromBounds(float half_em,
                                        const SkRect& bound,
                                        bool is_horizontal) {
  if (is_horizontal) {
    if (bound.right() <= half_em) {
      return HanKerning::CharType::kClose;
    }
    if (bound.left() >= half_em) {
      return HanKerning::CharType::kOpen;
    }
    if (bound.width() <= half_em && bound.left() >= half_em / 2) {
      return HanKerning::CharType::kMiddle;
    }
  } else {
    if (bound.bottom() <= half_em) {
      return HanKerning::CharType::kClose;
    }
    if (bound.top() >= half_em) {
      return HanKerning::CharType::kOpen;
    }
    if (bound.height() <= half_em && bound.top() >= half_em / 2) {
      return HanKerning::CharType::kMiddle;
    }
  }
  return HanKerning::CharType::kOther;
}

HanKerning::CharType CharTypeFromBounds(
    base::span<HarfBuzzShaper::GlyphData> glyphs,
    base::span<SkRect> bounds,
    unsigned index,
    bool is_horizontal) {
  const HarfBuzzShaper::GlyphData& glyph = glyphs[index];
  if (!glyph.glyph) [[unlikely]] {
    return HanKerning::CharType::kOther;
  }
  const float advance = GetAdvance(glyph, is_horizontal);
  return CharTypeFromBounds(advance / 2, bounds[index], is_horizontal);
}

HanKerning::CharType CharTypeFromBounds(
    base::span<HarfBuzzShaper::GlyphData> glyphs,
    base::span<SkRect> bounds,
    bool is_horizontal) {
  DCHECK_EQ(glyphs.size(), bounds.size());

  // Find the data from the first glyph.
  float advance0;
  float half_advance0;
  HanKerning::CharType type0 = HanKerning::CharType::kOther;
  unsigned i = 0;
  for (;; ++i) {
    if (i >= glyphs.size()) [[unlikely]] {
      return HanKerning::CharType::kOther;
    }
    const HarfBuzzShaper::GlyphData& glyph = glyphs[i];
    if (!glyph.glyph) [[unlikely]] {
      continue;
    }

    advance0 = GetAdvance(glyph, is_horizontal);
    half_advance0 = advance0 / 2;
    type0 = CharTypeFromBounds(half_advance0, bounds[i], is_horizontal);
    break;
  }

  // Check if all other glyphs have the same advances and types.
  for (++i; i < glyphs.size(); ++i) {
    const HarfBuzzShaper::GlyphData& glyph = glyphs[i];
    if (!glyph.glyph) [[unlikely]] {
      continue;
    }

    // If advances are not the same, `kOther`.
    const float advance = GetAdvance(glyph, is_horizontal);
    if (advance != advance0) {
      return HanKerning::CharType::kOther;
    }

    // If types are not the same, `kOther`.
    const HanKerning::CharType type =
        CharTypeFromBounds(half_advance0, bounds[i], is_horizontal);
    if (type != type0) {
      return HanKerning::CharType::kOther;
    }
  }
  return type0;
}

}  // namespace

void HanKerning::ResetFeatures() {
  DCHECK(features_);
#if EXPENSIVE_DCHECKS_ARE_ON()
  for (wtf_size_t i = num_features_before_; i < features_->size(); ++i) {
    const hb_feature_t& feature = (*features_)[i];
    DCHECK(feature.tag == HB_TAG('h', 'a', 'l', 't') ||
           feature.tag == HB_TAG('v', 'h', 'a', 'l'));
  }
#endif
  features_->Shrink(num_features_before_);
}

// Compute the character class.
// See Text Spacing Character Classes:
// https://drafts.csswg.org/css-text-4/#text-spacing-classes
HanKerning::CharType HanKerning::GetCharType(UChar ch,
                                             const FontData& font_data) {
  const CharType type = Character::GetHanKerningCharType(ch);
  switch (type) {
    case CharType::kOther:
    case CharType::kOpen:
    case CharType::kClose:
    case CharType::kMiddle:
    case CharType::kOpenNarrow:
    case CharType::kCloseNarrow:
      return type;
    case CharType::kDot:
      return font_data.type_for_dot;
    case CharType::kColon:
      return font_data.type_for_colon;
    case CharType::kSemicolon:
      return font_data.type_for_semicolon;
    case CharType::kOpenQuote:
      return font_data.is_quote_fullwidth ? CharType::kOpen
                                          : CharType::kOpenNarrow;
    case CharType::kCloseQuote:
      return font_data.is_quote_fullwidth ? CharType::kClose
                                          : CharType::kCloseNarrow;
  }
  NOTREACHED();
}

bool HanKerning::MayApply(StringView text) {
  return !text.Is8Bit() && !text.IsAllSpecialCharacters<[](UChar ch) {
    return !Character::MaybeHanKerningOpenOrCloseFast(ch);
  }>();
}

inline bool HanKerning::ShouldKern(CharType type, CharType last_type) {
  return type == CharType::kOpen &&
         (last_type == CharType::kOpen || last_type == CharType::kMiddle ||
          last_type == CharType::kClose || last_type == CharType::kOpenNarrow);
}

inline bool HanKerning::ShouldKernLast(CharType type, CharType last_type) {
  return last_type == CharType::kClose &&
         (type == CharType::kClose || type == CharType::kMiddle ||
          type == CharType::kCloseNarrow);
}

// Compute kerning and apply features.
// See Fullwidth Punctuation Collapsing:
// https://drafts.csswg.org/css-text-4/#fullwidth-collapsing
void HanKerning::Compute(const String& text,
                         wtf_size_t start,
                         wtf_size_t end,
                         const SimpleFontData& font,
                         const FontDescription& font_description,
                         Options options,
                         FontFeatures* features) {
  DCHECK(!features_);
  DCHECK_GT(end, start);
  if (!MayApply(StringView(text, start, end - start))) {
    return;
  }
  const LayoutLocale& locale = font_description.LocaleOrDefault();
  const FontData& font_data =
      font.HanKerningData(locale, options.is_horizontal);
  if (!font_data.has_alternate_spacing) {
    return;
  }
  if (font_description.GetTextSpacingTrim() == TextSpacingTrim::kSpaceAll)
      [[unlikely]] {
    return;
  }
  for (const hb_feature_t& feature : *features) {
    if (feature.value && IsExclusiveFeature(feature.tag)) {
      return;
    }
  }

  // Compute for the first character.
  Vector<wtf_size_t, 32> indices;
  CharType last_type;
  if (options.apply_start) [[unlikely]] {
    indices.push_back(start);
    unsafe_to_break_before_.push_back(start);
    last_type = GetCharType(text[start], font_data);
  } else if (start && !options.is_line_start) {
    last_type = GetCharType(text[start - 1], font_data);
    const CharType type = GetCharType(text[start], font_data);
    if (ShouldKern(type, last_type)) {
      indices.push_back(start);
      unsafe_to_break_before_.push_back(start);
    }
    last_type = type;
  } else {
    last_type = GetCharType(text[start], font_data);
  }

  if (font_data.has_contextual_spacing) {
    // The `chws` feature can handle charcters in a run.
    // Compute the end edge if there are following runs.
    if (options.apply_end) [[unlikely]] {
      indices.push_back(end - 1);
    } else if (end < text.length()) {
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
        unsafe_to_break_before_.push_back(i);
      } else if (ShouldKern(type, last_type)) {
        indices.push_back(i);
        unsafe_to_break_before_.push_back(i);
      }
    }

    // Compute for the last character.
    if (options.apply_end) [[unlikely]] {
      indices.push_back(end - 1);
    } else if (end < text.length()) {
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
  const hb_tag_t tag = options.is_horizontal ? HB_TAG('h', 'a', 'l', 't')
                                             : HB_TAG('v', 'h', 'a', 'l');
  features_ = features;
  num_features_before_ = features->size();
  features->Reserve(features->size() + indices.size());
  for (const wtf_size_t i : indices) {
    features->Append({tag, 1, i, i + 1});
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
      kFullwidthColon, kFullwidthSemicolon,
      // Quote characters. In a common convention, they are proportional (Latin)
      // in Japanese, but fullwidth in Chinese.
      kLeftDoubleQuotationMarkCharacter, kLeftSingleQuotationMarkCharacter,
      kRightDoubleQuotationMarkCharacter, kRightSingleQuotationMarkCharacter};
  constexpr unsigned kDotSize = 4;
  constexpr unsigned kColonIndex = 4;
  constexpr unsigned kSemicolonIndex = 5;
  constexpr unsigned kQuoteStartIndex = 6;
  static_assert(kDotSize <= std::size(kChars));
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

  // If the font doesn't have any of these glyphs, or uses multiple glyphs for a
  // code point, it's not applicable.
  if (glyph_data_list.size() != std::size(kChars)) {
    has_alternate_spacing = false;
    return;
  }

  Vector<Glyph, 256> glyphs;
  unsigned cluster = 0;
  for (const HarfBuzzShaper::GlyphData& glyph_data : glyph_data_list) {
    if (glyph_data.cluster != cluster) [[unlikely]] {
      has_alternate_spacing = false;
      return;
    }
    ++cluster;
    glyphs.push_back(glyph_data.glyph);
  }

  // Compute glyph bounds for all glyphs.
  Vector<SkRect, 256> bounds(glyphs.size());
  font.BoundsForGlyphs(glyphs, &bounds);

  // `bounds` are relative to the glyph origin. Adjust them to be relative to
  // the paint origin.
  DCHECK_LE(bounds.size(), glyph_data_list.size());
  for (wtf_size_t i = 0; i < bounds.size(); ++i) {
    const HarfBuzzShaper::GlyphData& glyph_data = glyph_data_list[i];
    bounds[i].offset({glyph_data.offset.x(), glyph_data.offset.y()});
  }

  // Compute types from glyph bounds.
  //
  // This logic allows each group of glyphs to have different advances, such as
  // when comma and full stop are narrower than `1ch`, as long as:
  // * The font has the `halt` feature.
  // * Glyphs in each group have the same advances.
  // * Glyphs have enough space to apply kerning.
  base::span<HarfBuzzShaper::GlyphData> glyph_data_span(glyph_data_list);
  base::span<SkRect> bounds_span(bounds);
  type_for_dot = CharTypeFromBounds(glyph_data_span.first(kDotSize),
                                    bounds_span.first(kDotSize), is_horizontal);
  type_for_colon = CharTypeFromBounds(glyph_data_span, bounds_span, kColonIndex,
                                      is_horizontal);
  type_for_semicolon = CharTypeFromBounds(glyph_data_span, bounds_span,
                                          kSemicolonIndex, is_horizontal);

  // Quotes not being fullwidth doesn't necessarily mean the font isn't
  // applicable. Quotes are unified by the Unicode unification process (i.e.,
  // Latin curly quotes and CJK quotes have the same code points,) and that they
  // can be either proportional or fullwidth. Japanese fonts oten have
  // proportional glyphs, prioritizing Latin usages, while Chinese fonts often
  // have fullwidth glyphs, prioritizing Chinese usages.
  //
  // Adobe has a convention to switch to CJK glyphs by the OpenType `fwid`
  // feature, but not all fonts follow this convention. The current logic
  // doesn't support this convention.
  //
  // Quotes are often misplaced, especially in Japanese vertical flow, due to
  // the lack of established conventions. In that case, treat such quotes the
  // same as narrow quotes. See `HanKerning::GetCharType`.
  is_quote_fullwidth = true;
  glyph_data_span = glyph_data_span.subspan(kQuoteStartIndex);
  bounds_span = bounds_span.subspan(kQuoteStartIndex);
  DCHECK_EQ(bounds_span.size(), 4u);
  if (CharTypeFromBounds(glyph_data_span.first(2u), bounds_span.first(2u),
                         is_horizontal) != CharType::kOpen ||
      CharTypeFromBounds(glyph_data_span.subspan(2u), bounds_span.subspan(2u),
                         is_horizontal) != CharType::kClose) {
    is_quote_fullwidth = false;
  }
}

}  // namespace blink
