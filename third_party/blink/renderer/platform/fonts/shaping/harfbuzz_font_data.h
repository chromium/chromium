// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FONT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FONT_DATA_H_

#include <hb-cplusplus.hh>
#include <memory>

#include "base/check_op.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_vertical_data.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkStrikeRef.h"

struct hb_font_t;

namespace blink {

const unsigned kInvalidFallbackMetricsValue = static_cast<unsigned>(-1);

// The HarfBuzzFontData struct carries user-pointer data for
// |hb_font_t| callback functions/operations. It contains metrics and OpenType
// layout information related to a font scaled to a particular size.
struct HarfBuzzFontData final : public GarbageCollected<HarfBuzzFontData> {
 public:
  explicit HarfBuzzFontData(hb_font_t* unscaled_font)
      : unscaled_font_(hb::unique_ptr<hb_font_t>(unscaled_font)),
        vertical_data_(nullptr),
        range_set_(nullptr) {}

  HarfBuzzFontData(const HarfBuzzFontData&) = delete;
  HarfBuzzFontData& operator=(const HarfBuzzFontData&) = delete;

  void Trace(Visitor* visitor) const {
    visitor->Trace(vertical_data_);
    visitor->Trace(range_set_);
  }

  // The vertical origin and vertical advance functions in HarfBuzzFace require
  // the ascent and height metrics as fallback in case no specific vertical
  // layout information is found from the font.
  void UpdateFallbackMetricsAndScale(
      const FontPlatformData& platform_data,
      HarfBuzzFace::VerticalLayoutCallbacks vertical_layout) {
    float ascent = 0;
    float descent = 0;

    SkFont new_font = platform_data.CreateSkFont();

    // Strikes are based on font data, so if the font changes, we need to reset
    // the strike data.
    if (strike_ref_ && font_ != new_font) {
      strike_ref_ = SkStrikeRef();
    }
    font_ = std::move(new_font);

    if (vertical_layout == HarfBuzzFace::kPrepareForVerticalLayout)
        [[unlikely]] {
      FontMetrics::AscentDescentWithHacks(ascent, descent, platform_data,
                                          font_);
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

  SkStrikeRef& EnsureStrikeRef() {
    if (!strike_ref_) {
      strike_ref_ = font_.makeStrikeRef();
    }
    return strike_ref_;
  }

  OpenTypeVerticalData* VerticalData() {
    if (!vertical_data_) {
      DCHECK_NE(ascent_fallback_, kInvalidFallbackMetricsValue);
      DCHECK_NE(height_fallback_, kInvalidFallbackMetricsValue);
      DCHECK_NE(size_per_unit_, kInvalidFallbackMetricsValue);

      vertical_data_ =
          MakeGarbageCollected<OpenTypeVerticalData>(font_.refTypeface());
    }
    vertical_data_->SetScaleAndFallbackMetrics(size_per_unit_, ascent_fallback_,
                                               height_fallback_);
    return vertical_data_.Get();
  }

  const hb::unique_ptr<hb_font_t> unscaled_font_;
  SkFont font_;
  // Lazily-populated cached strike for the HarfBuzz advance callbacks; reset
  // when `font_` changes. See `UpdateFallbackMetricsAndScale`.
  SkStrikeRef strike_ref_;

  // Capture these scaled fallback metrics from FontPlatformData so that a
  // OpenTypeVerticalData object can be constructed from them when needed.
  float size_per_unit_;
  float ascent_fallback_;
  float height_fallback_;

  enum class SpaceGlyphInOpenTypeTables { kUnknown, kPresent, kNotPresent };

  SpaceGlyphInOpenTypeTables space_in_gpos_ =
      SpaceGlyphInOpenTypeTables::kUnknown;
  SpaceGlyphInOpenTypeTables space_in_gsub_ =
      SpaceGlyphInOpenTypeTables::kUnknown;

  Member<OpenTypeVerticalData> vertical_data_;
  Member<const UnicodeRangeSet> range_set_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_HARFBUZZ_FONT_DATA_H_
