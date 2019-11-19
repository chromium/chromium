// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FONT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FONT_DATA_H_

#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_vertical_data.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/skia/include/core/SkFont.h"

struct hb_font_t;

namespace blink {

const unsigned kInvalidFallbackMetricsValue = static_cast<unsigned>(-1);

// The HarfBuzzFontData struct carries user-pointer data for hb_font_t callback
// functions/operations. It contains metrics and OpenType layout information
// related to a font scaled to a particular size.
struct HarfBuzzFontData {
  USING_FAST_MALLOC(HarfBuzzFontData);

 public:
  HarfBuzzFontData()
      : font_(),
        space_in_gpos_(SpaceGlyphInOpenTypeTables::Unknown),
        space_in_gsub_(SpaceGlyphInOpenTypeTables::Unknown),
        vertical_data_(nullptr),
        range_set_(nullptr) {}

  // The vertical origin and vertical advance functions in HarfBuzzFace require
  // the ascent and height metrics as fallback in case no specific vertical
  // layout information is found from the font.
  void UpdateFallbackMetricsAndScale(
      const FontPlatformData& platform_data,
      HarfBuzzFace::VerticalLayoutCallbacks vertical_layout) {
    float ascent = 0;
    float descent = 0;
    unsigned dummy_ascent_inflation = 0;
    unsigned dummy_descent_inflation = 0;

    font_ = SkFont();
    platform_data.SetupSkFont(&font_);

    if (UNLIKELY(vertical_layout == HarfBuzzFace::PrepareForVerticalLayout)) {
      FontMetrics::AscentDescentWithHacks(
          ascent, descent, dummy_ascent_inflation, dummy_descent_inflation,
          platform_data, font_);
      ascent_fallback_ = ascent;
      // Simulate the rounding that FontMetrics does so far for returning the
      // integer Height()
      height_fallback_ = lroundf(ascent) + lroundf(descent);

      int units_per_em =
          platform_data.GetHarfBuzzFace()->UnitsPerEmFromHeadTable();
      if (!units_per_em) {
        DLOG(ERROR)
            << "Units per EM is 0 for font used in vertical writing mode.";
      }
      size_per_unit_ = platform_data.size() / (units_per_em ? units_per_em : 1);
    } else {
      ascent_fallback_ = kInvalidFallbackMetricsValue;
      height_fallback_ = kInvalidFallbackMetricsValue;
      size_per_unit_ = kInvalidFallbackMetricsValue;
    }
  }

  float SizePerUnit(const SkTypeface& typeface) const {
    if (size_per_unit_ != kInvalidFallbackMetricsValue)
      return size_per_unit_;
    int units_per_em = typeface.getUnitsPerEm();
    size_per_unit_ = font_.getSize() / units_per_em;
    return size_per_unit_;
  }

  scoped_refptr<OpenTypeVerticalData> VerticalData() {
    if (!vertical_data_) {
      DCHECK_NE(ascent_fallback_, kInvalidFallbackMetricsValue);
      DCHECK_NE(height_fallback_, kInvalidFallbackMetricsValue);
      DCHECK_NE(size_per_unit_, kInvalidFallbackMetricsValue);

      vertical_data_ =
          OpenTypeVerticalData::CreateUnscaled(font_.refTypeface());
    }
    vertical_data_->SetScaleAndFallbackMetrics(size_per_unit_, ascent_fallback_,
                                               height_fallback_);
    return vertical_data_;
  }

  SkFont font_;

  // Capture these scaled fallback metrics from FontPlatformData so that a
  // OpenTypeVerticalData object can be constructed from them when needed.
  mutable float size_per_unit_;
  float ascent_fallback_;
  float height_fallback_;

  enum class SpaceGlyphInOpenTypeTables { Unknown, Present, NotPresent };

  SpaceGlyphInOpenTypeTables space_in_gpos_;
  SpaceGlyphInOpenTypeTables space_in_gsub_;

  scoped_refptr<OpenTypeVerticalData> vertical_data_;
  scoped_refptr<UnicodeRangeSet> range_set_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HarfBuzzFontData);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FONT_DATA_H_
