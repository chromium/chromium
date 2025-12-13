// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/han_kerning.h"

#include <unicode/uchar.h>

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_features.h"
#include "third_party/blink/renderer/platform/fonts/shaping/font_features.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_cursor.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_run.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
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
  DEFINE_THREAD_SAFE_STATIC_LOCAL(HashSet<uint32_t>, tags,
                                  (ExclusiveFeatures()));
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
    case CharType::kInvalid:
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

HanKerning::CharType HanKerning::GetCharTypeWithCache(const String& text,
                                                      wtf_size_t index,
                                                      const FontData& font_data,
                                                      Priority priority) {
  DCHECK(RuntimeEnabledFeatures::TextSpacingTrimFallbackEnabled());
  DCHECK(!char_types_.empty());
  const CharType cached_type = char_types_[index];
  if (priority == Priority::kCache && cached_type != CharType::kInvalid) {
    return cached_type;
  }
  const CharType type = GetCharType(text[index], font_data);
  if (type == cached_type) {
    return type;
  }

  char_types_[index] = type;

  // The `CharType` becomes different due to font changes. If it causes
  // different kerning for the next or previous characters, keep their indexes.
  if (cached_type != CharType::kInvalid &&
      RuntimeEnabledFeatures::TextSpacingTrimFallback2Enabled()) {
    if (index > segment_start_) {
      const CharType prev_type = char_types_[index - 1];
      if (prev_type != CharType::kInvalid && ShouldKernLast(type, prev_type) &&
          !ShouldKernLast(cached_type, prev_type)) {
        changed_indexes_.push_back(index - 1);
      }
    }
    if (index + 1 < segment_end_) {
      const CharType next_type = char_types_[index + 1];
      if (next_type != CharType::kInvalid && ShouldKern(next_type, type) &&
          !ShouldKern(next_type, cached_type)) {
        changed_indexes_.push_back(index + 1);
      }
    }
  }
  return type;
}

inline HanKerning::CharType HanKerning::GetCharType(const String& text,
                                                    wtf_size_t index,
                                                    const FontData& font_data,
                                                    Priority priority) {
  return char_types_.empty()
             ? GetCharType(text[index], font_data)
             : GetCharTypeWithCache(text, index, font_data, priority);
}

// Compute kerning and apply features.
// See Fullwidth Punctuation Collapsing:
// https://drafts.csswg.org/css-text-4/#fullwidth-collapsing
bool HanKerning::AppendFontFeatures(const String& text,
                                    wtf_size_t start,
                                    wtf_size_t end,
                                    const SimpleFontData& font,
                                    const LayoutLocale& locale,
                                    Options options,
                                    FontFeatureRanges& features) {
#if EXPENSIVE_DCHECKS_ARE_ON()
  DCHECK_GT(end, start);
  DCHECK_GE(start, segment_start_);
  DCHECK_LE(end, segment_end_);
  // Caller should check `MayApply`.
  DCHECK(MayApply(
      StringView(text, segment_start_, segment_end_ - segment_start_)));
#endif  // EXPENSIVE_DCHECKS_ARE_ON()

  if (start != segment_start_ || end != segment_end_) {
    if (!MayApply(StringView(text, start, end - start))) {
      return false;
    }
  }
  const FontData& font_data =
      font.HanKerningData(locale, options.is_horizontal);
  if (!font_data.has_alternate_spacing) {
    return false;
  }
  for (const FontFeatureRange& feature : features) {
    if (feature.value && IsExclusiveFeature(feature.tag)) [[unlikely]] {
      return false;
    }
  }

  last_start_ = start;
  last_end_ = end;
  is_start_prev_used_ = false;
  is_end_next_used_ = false;
  last_font_data_ = &font_data;

  // Compute for the first character.
  Vector<wtf_size_t, 32> indices;
  CharType last_type;
  if (options.apply_start) [[unlikely]] {
    indices.push_back(start);
    unsafe_to_break_before_.push_back(start);
    last_type = GetCharType(text, start, font_data);
  } else if (start && !options.is_line_start) {
    is_start_prev_used_ = true;
    last_type = GetCharType(text, start - 1, font_data, Priority::kCache);
    const CharType type = GetCharType(text, start, font_data);
    if (ShouldKern(type, last_type)) {
      indices.push_back(start);
      unsafe_to_break_before_.push_back(start);
    }
    last_type = type;
  } else {
    last_type = GetCharType(text, start, font_data);
  }

  if (font_data.has_contextual_spacing &&
      (char_types_.empty() ||
       RuntimeEnabledFeatures::TextSpacingTrimFallbackChwsEnabled())) {
    // The `chws` feature can handle characters in a run.
    // Compute the end edge if there are following runs.
    if (options.apply_end) [[unlikely]] {
      indices.push_back(end - 1);
    } else if (end < text.length()) {
      if (end - 1 > start) {
        last_type = GetCharType(text[end - 1], font_data);
      }
      is_end_next_used_ = true;
      const CharType type = GetCharType(text[end], font_data);
      if (ShouldKernLast(type, last_type)) {
        indices.push_back(end - 1);
      }
    }
  } else {
    // Compute for characters in the middle.
    CharType type;
    for (wtf_size_t i = start + 1; i < end; ++i, last_type = type) {
      type = GetCharType(text, i, font_data);
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
      is_end_next_used_ = true;
      type = GetCharType(text, end, font_data, Priority::kCache);
      if (ShouldKernLast(type, last_type)) {
        indices.push_back(end - 1);
      }
    }
  }

  // Append to `features`.
  if (indices.empty()) {
    return true;
  }
#if EXPENSIVE_DCHECKS_ARE_ON()
  DCHECK(std::is_sorted(indices.begin(), indices.end(), std::less_equal<>()));
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
  const hb_tag_t tag = options.is_horizontal ? HB_TAG('h', 'a', 'l', 't')
                                             : HB_TAG('v', 'h', 'a', 'l');
  features.reserve(features.size() + indices.size());
  for (const wtf_size_t i : indices) {
    features.push_back(FontFeatureRange{{tag, 1}, i, i + 1});
  }
  return true;
}

void HanKerning::PrepareFallback(const String& text) {
  DCHECK(RuntimeEnabledFeatures::TextSpacingTrimFallbackEnabled());
  if (char_types_.empty()) {
    char_types_.Grow(text.length());
    char_types_.Fill(CharType::kInvalid);
  } else {
    DCHECK_EQ(text.length(), char_types_.size());
  }

  wtf_size_t start = last_start_;
  if (is_start_prev_used_ && char_types_[start - 1] == CharType::kInvalid) {
    --start;
  }
  wtf_size_t end = last_end_;
  if (is_end_next_used_ && char_types_[end] == CharType::kInvalid) {
    ++end;
  }
  for (wtf_size_t i = start; i < end; ++i) {
    char_types_[i] = GetCharType(text[i], *last_font_data_);
  }
}

// Apply kerning to indexes where actual `CharType`s are different from
// predicted `CharType`s. Features can't be applied because shaping is already
// done. Adjust letter spacing instead.
void HanKerning::ApplyKerning(ShapeResult& result) {
  DCHECK(!changed_indexes_.empty());
  DCHECK(RuntimeEnabledFeatures::TextSpacingTrimFallback2Enabled());

  ShapeResultCursor cursor(&result);
  const float font_size = cursor.FontData().PlatformData().size();
  const auto advance_min = TextRunLayoutUnit::FromFloatFloor(font_size * .9);
  std::sort(changed_indexes_.begin(), changed_indexes_.end());
  wtf_size_t last_index = kNotFound;
  for (wtf_size_t i : changed_indexes_) {
    if (i == last_index) [[unlikely]] {
      continue;
    }
    last_index = i;

    cursor.MoveToCharacter(i);
    const TextRunLayoutUnit advance = cursor.ClusterAdvance();
    if (advance < advance_min) {
      continue;
    }

    cursor.SetUnsafeToBreakBefore();
    switch (char_types_[i]) {
      case CharType::kOpen:
        cursor.AddSpaceToLeft(advance / -2);
        break;
      case CharType::kClose:
        cursor.AddSpaceToRight(advance / -2);
        break;
      default:
        NOTREACHED();
    }
  }
  changed_indexes_.Shrink(0);
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

#if BUILDFLAG(IS_WIN)
  if (RuntimeEnabledFeatures::TextSpacingTrimYuGothicUIEnabled()) {
    // Exclude "Yu Gothic UI" until the fonts are fixed. crbug.com/331123676
    const String postscript_name = font.PlatformData().GetPostScriptName();
    if (postscript_name.StartsWith("YuGothicUI")) [[unlikely]] {
      has_alternate_spacing = false;
      return;
    }
  }
#endif  // BUILDFLAG(IS_WIN)

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
      uchar::kIdeographicComma, uchar::kIdeographicFullStop,
      uchar::kFullwidthComma, uchar::kFullwidthFullStop,
      // Colon characters.
      // https://drafts.csswg.org/css-text-4/#fullwidth-colon-punctuation
      uchar::kFullwidthColon, uchar::kFullwidthSemicolon,
      // Quote characters. In a common convention, they are proportional (Latin)
      // in Japanese, but fullwidth in Chinese.
      uchar::kLeftDoubleQuotationMark, uchar::kLeftSingleQuotationMark,
      uchar::kRightDoubleQuotationMark, uchar::kRightSingleQuotationMark};
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
  HarfBuzzShaper shaper{String(base::span(kChars))};
  HarfBuzzShaper::GlyphDataList glyph_data_list;
  shaper.GetGlyphData(font, locale, locale.GetScriptForHan(), is_horizontal,
                      TextDirection::kLtr, glyph_data_list);

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
