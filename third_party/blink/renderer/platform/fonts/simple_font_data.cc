/*
 * Copyright (C) 2005, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"

#include <unicode/utf16.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/skia/skia_text_metrics.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/skia/include/core/SkFontMetrics.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace blink {

const float kSmallCapsFontSizeMultiplier = 0.7f;
const float kEmphasisMarkFontSizeMultiplier = 0.5f;

SimpleFontData::SimpleFontData(const FontPlatformData& platform_data,
                               scoped_refptr<CustomFontData> custom_data,
                               bool subpixel_ascent_descent,
                               const FontMetricsOverride& metrics_override)
    : max_char_width_(-1),
      avg_char_width_(-1),
      platform_data_(platform_data),
      custom_font_data_(std::move(custom_data)),
      visual_overflow_inflation_for_ascent_(0),
      visual_overflow_inflation_for_descent_(0) {
  PlatformInit(subpixel_ascent_descent, metrics_override);
  PlatformGlyphInit();
}

void SimpleFontData::PlatformInit(bool subpixel_ascent_descent,
                                  const FontMetricsOverride& metrics_override) {
  if (!platform_data_.size()) {
    font_metrics_.Reset();
    avg_char_width_ = 0;
    max_char_width_ = 0;
    return;
  }

  SkFontMetrics metrics;

  font_ = SkFont();
  platform_data_.SetupSkFont(&font_);
  font_.getMetrics(&metrics);

  float ascent;
  float descent;

  FontMetrics::AscentDescentWithHacks(
      ascent, descent, visual_overflow_inflation_for_ascent_,
      visual_overflow_inflation_for_descent_, platform_data_, font_,
      subpixel_ascent_descent, metrics_override.ascent_override,
      metrics_override.descent_override);

  font_metrics_.SetAscent(ascent);
  font_metrics_.SetDescent(descent);

  float skia_underline_value;
  if (metrics.hasUnderlinePosition(&skia_underline_value))
    font_metrics_.SetUnderlinePosition(skia_underline_value);
  if (metrics.hasUnderlineThickness(&skia_underline_value))
    font_metrics_.SetUnderlineThickness(skia_underline_value);

  float x_height;
  if (metrics.fXHeight) {
    x_height = metrics.fXHeight;
#if defined(OS_MAC)
    // Mac OS CTFontGetXHeight reports the bounding box height of x,
    // including parts extending below the baseline and apparently no x-height
    // value from the OS/2 table. However, the CSS ex unit
    // expects only parts above the baseline, hence measuring the glyph:
    // http://www.w3.org/TR/css3-values/#ex-unit
    const Glyph x_glyph = GlyphForCharacter('x');
    if (x_glyph) {
      FloatRect glyph_bounds(BoundsForGlyph(x_glyph));
      // SkGlyph bounds, y down, based on rendering at (0,0).
      x_height = -glyph_bounds.Y();
    }
#endif
    font_metrics_.SetXHeight(x_height);
  } else {
    x_height = ascent * 0.56;  // Best guess from Windows font metrics.
    font_metrics_.SetXHeight(x_height);
    font_metrics_.SetHasXHeight(false);
  }

  float line_gap;
  if (metrics_override.line_gap_override) {
    line_gap = *metrics_override.line_gap_override * platform_data_.size();
  } else {
    line_gap = SkScalarToFloat(metrics.fLeading);
  }
  font_metrics_.SetLineGap(line_gap);
  font_metrics_.SetLineSpacing(lroundf(ascent) + lroundf(descent) +
                               lroundf(line_gap));

// In WebKit/WebCore/platform/graphics/SimpleFontData.cpp, m_spaceWidth is
// calculated for us, but we need to calculate m_maxCharWidth and
// m_avgCharWidth in order for text entry widgets to be sized correctly.
#if defined(OS_WIN)
  max_char_width_ = SkScalarRoundToInt(metrics.fMaxCharWidth);

  // Older version of the DirectWrite API doesn't implement support for max
  // char width. Fall back on a multiple of the ascent. This is entirely
  // arbitrary but comes pretty close to the expected value in most cases.
  if (max_char_width_ < 1)
    max_char_width_ = ascent * 2;
#elif defined(OS_MAC)
  // FIXME: The current avg/max character width calculation is not ideal,
  // it should check either the OS2 table or, better yet, query FontMetrics.
  // Sadly FontMetrics provides incorrect data on Mac at the moment.
  // https://crbug.com/420901
  max_char_width_ = std::max(avg_char_width_, font_metrics_.FloatAscent());
#else
  // Better would be to rely on either fMaxCharWidth or fAveCharWidth.
  // skbug.com/3087
  max_char_width_ = SkScalarRoundToInt(metrics.fXMax - metrics.fXMin);

#endif

#if !defined(OS_MAC)
  if (metrics.fAvgCharWidth) {
    avg_char_width_ = SkScalarRoundToInt(metrics.fAvgCharWidth);
  } else {
#endif
    avg_char_width_ = x_height;
    const Glyph x_glyph = GlyphForCharacter('x');
    if (x_glyph) {
      avg_char_width_ = WidthForGlyph(x_glyph);
    }
#if !defined(OS_MAC)
  }
#endif

  SkTypeface* face = font_.getTypeface();
  DCHECK(face);
  if (int units_per_em = face->getUnitsPerEm())
    font_metrics_.SetUnitsPerEm(units_per_em);

  if (metrics_override.advance_override) {
    advance_override_ =
        *metrics_override.advance_override * platform_data_.size();
  }

  advance_proportional_override_ =
      metrics_override.advance_proportional_override;
}

void SimpleFontData::PlatformGlyphInit() {
  SkTypeface* typeface = PlatformData().Typeface();
  if (!typeface->countGlyphs()) {
    space_glyph_ = 0;
    space_width_ = 0;
    zero_glyph_ = 0;
    return;
  }

  // Nasty hack to determine if we should round or ceil space widths.
  // If the font is monospace or fake monospace we ceil to ensure that
  // every character and the space are the same width.  Otherwise we round.
  space_glyph_ = GlyphForCharacter(' ');
  float width = WidthForGlyph(space_glyph_);
  space_width_ = width;
  zero_glyph_ = GlyphForCharacter('0');
  font_metrics_.SetZeroWidth(WidthForGlyph(zero_glyph_));
}

const SimpleFontData* SimpleFontData::FontDataForCharacter(UChar32) const {
  return this;
}

Glyph SimpleFontData::GlyphForCharacter(UChar32 codepoint) const {
  SkTypeface* typeface = PlatformData().Typeface();
  CHECK(typeface);
  return typeface->unicharToGlyph(codepoint);
}

bool SimpleFontData::IsSegmented() const {
  return false;
}

scoped_refptr<SimpleFontData> SimpleFontData::SmallCapsFontData(
    const FontDescription& font_description) const {
  if (!derived_font_data_)
    derived_font_data_ = std::make_unique<DerivedFontData>();
  if (!derived_font_data_->small_caps)
    derived_font_data_->small_caps =
        CreateScaledFontData(font_description, kSmallCapsFontSizeMultiplier);

  return derived_font_data_->small_caps;
}

scoped_refptr<SimpleFontData> SimpleFontData::EmphasisMarkFontData(
    const FontDescription& font_description) const {
  if (!derived_font_data_)
    derived_font_data_ = std::make_unique<DerivedFontData>();
  if (!derived_font_data_->emphasis_mark)
    derived_font_data_->emphasis_mark =
        CreateScaledFontData(font_description, kEmphasisMarkFontSizeMultiplier);

  return derived_font_data_->emphasis_mark;
}

scoped_refptr<SimpleFontData> SimpleFontData::CreateScaledFontData(
    const FontDescription& font_description,
    float scale_factor) const {
  const float scaled_size =
      lroundf(font_description.ComputedSize() * scale_factor);
  return SimpleFontData::Create(
      FontPlatformData(platform_data_, scaled_size),
      IsCustomFont() ? CustomFontData::Create() : nullptr);
}

scoped_refptr<SimpleFontData> SimpleFontData::MetricsOverriddenFontData(
    const FontMetricsOverride& metrics_override) const {
  return base::AdoptRef(new SimpleFontData(platform_data_, custom_font_data_,
                                           false /* subpixel_ascent_descent */,
                                           metrics_override));
}

// Internal leadings can be distributed to ascent and descent.
// -------------------------------------------
//           | - Internal Leading (in ascent)
//           |--------------------------------
//  Ascent - |              |
//           |              |
//           |              | - Em height
// ----------|--------------|
//           |              |
// Descent - |--------------------------------
//           | - Internal Leading (in descent)
// -------------------------------------------
FontHeight SimpleFontData::NormalizedTypoAscentAndDescent(
    FontBaseline baseline_type) const {
  if (baseline_type == kAlphabeticBaseline) {
    if (!normalized_typo_ascent_descent_.ascent)
      ComputeNormalizedTypoAscentAndDescent();
    return normalized_typo_ascent_descent_;
  }
  const LayoutUnit normalized_height =
      LayoutUnit::FromFloatRound(PlatformData().size());
  return {normalized_height - normalized_height / 2, normalized_height / 2};
}

LayoutUnit SimpleFontData::NormalizedTypoAscent(
    FontBaseline baseline_type) const {
  return NormalizedTypoAscentAndDescent(baseline_type).ascent;
}

LayoutUnit SimpleFontData::NormalizedTypoDescent(
    FontBaseline baseline_type) const {
  return NormalizedTypoAscentAndDescent(baseline_type).descent;
}

static std::pair<int16_t, int16_t> TypoAscenderAndDescender(
    SkTypeface* typeface) {
  // TODO(kojii): This should move to Skia once finalized. We can then move
  // EmHeightAscender/Descender to FontMetrics.
  int16_t buffer[2];
  size_t size = typeface->getTableData(SkSetFourByteTag('O', 'S', '/', '2'), 68,
                                       sizeof(buffer), buffer);
  if (size == sizeof(buffer)) {
    return std::make_pair(static_cast<int16_t>(base::NetToHost16(buffer[0])),
                          -static_cast<int16_t>(base::NetToHost16(buffer[1])));
  }
  return std::make_pair(0, 0);
}

void SimpleFontData::ComputeNormalizedTypoAscentAndDescent() const {
  // Compute em height metrics from OS/2 sTypoAscender and sTypoDescender.
  SkTypeface* typeface = platform_data_.Typeface();
  int16_t typo_ascender, typo_descender;
  std::tie(typo_ascender, typo_descender) = TypoAscenderAndDescender(typeface);
  if (typo_ascender > 0 &&
      TrySetNormalizedTypoAscentAndDescent(typo_ascender, typo_descender)) {
    return;
  }

  // As the last resort, compute em height metrics from our ascent/descent.
  const FontMetrics& font_metrics = GetFontMetrics();
  if (TrySetNormalizedTypoAscentAndDescent(font_metrics.FloatAscent(),
                                           font_metrics.FloatDescent())) {
    return;
  }
  NOTREACHED();
}

bool SimpleFontData::TrySetNormalizedTypoAscentAndDescent(float ascent,
                                                          float descent) const {
  const float height = ascent + descent;
  if (height <= 0 || ascent < 0 || ascent > height)
    return false;
  // While the OpenType specification recommends the sum of sTypoAscender and
  // sTypoDescender to equal 1em, most fonts do not follow. Most Latin fonts
  // set to smaller than 1em, and many tall scripts set to larger than 1em.
  // https://www.microsoft.com/typography/otspec/recom.htm#OS2
  // To ensure the sum of ascent and descent is the "em height", normalize by
  // keeping the ratio of sTypoAscender:sTypoDescender.
  // This matches to how Gecko computes "em height":
  // https://github.com/whatwg/html/issues/2470#issuecomment-291425136
  const float em_height = PlatformData().size();
  const LayoutUnit normalized_ascent =
      LayoutUnit::FromFloatRound(ascent * em_height / height);
  normalized_typo_ascent_descent_ = {
      normalized_ascent,
      LayoutUnit::FromFloatRound(em_height) - normalized_ascent};
  return true;
}

LayoutUnit SimpleFontData::VerticalPosition(
    FontVerticalPositionType position_type,
    FontBaseline baseline_type) const {
  switch (position_type) {
    case FontVerticalPositionType::TextTop:
      // Use Ascent, not FixedAscent, to match to how painter computes the
      // baseline position.
      return LayoutUnit(GetFontMetrics().Ascent(baseline_type));
    case FontVerticalPositionType::TextBottom:
      return LayoutUnit(-GetFontMetrics().Descent(baseline_type));
    case FontVerticalPositionType::TopOfEmHeight:
      return NormalizedTypoAscent(baseline_type);
    case FontVerticalPositionType::BottomOfEmHeight:
      return -NormalizedTypoDescent(baseline_type);
  }
  NOTREACHED();
  return LayoutUnit();
}

FloatRect SimpleFontData::PlatformBoundsForGlyph(Glyph glyph) const {
  if (!platform_data_.size())
    return FloatRect();

  static_assert(sizeof(glyph) == 2, "Glyph id should not be truncated.");

  SkRect bounds;
  SkFontGetBoundsForGlyph(font_, glyph, &bounds);
  return FloatRect(bounds);
}

void SimpleFontData::BoundsForGlyphs(const Vector<Glyph, 256>& glyphs,
                                     Vector<SkRect, 256>* bounds) const {
  DCHECK_EQ(glyphs.size(), bounds->size());

  if (!platform_data_.size())
    return;

  DCHECK_EQ(bounds->size(), glyphs.size());
  SkFontGetBoundsForGlyphs(font_, glyphs, bounds->data());
}

float SimpleFontData::PlatformWidthForGlyph(Glyph glyph) const {
  if (!platform_data_.size())
    return 0;

  static_assert(sizeof(glyph) == 2, "Glyph id should not be truncated.");

  return SkFontGetWidthForGlyph(font_, glyph);
}

}  // namespace blink
