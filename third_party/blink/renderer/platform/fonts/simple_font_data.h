/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SIMPLE_FONT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SIMPLE_FONT_DATA_H_

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/canvas_rotation_in_vertical.h"
#include "third_party/blink/renderer/platform/fonts/custom_font_data.h"
#include "third_party/blink/renderer/platform/fonts/font_baseline.h"
#include "third_party/blink/renderer/platform/fonts/font_data.h"
#include "third_party/blink/renderer/platform/fonts/font_metrics.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_vertical_position_type.h"
#include "third_party/blink/renderer/platform/fonts/glyph.h"
#include "third_party/blink/renderer/platform/fonts/typesetting_features.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/skia/include/core/SkFont.h"

#if defined(OS_MACOSX)
#include "third_party/blink/renderer/platform/fonts/glyph_metrics_map.h"
#endif

namespace blink {

// Holds the glyph index and the corresponding SimpleFontData information for a
// given
// character.
struct GlyphData {
  STACK_ALLOCATED();

 public:
  GlyphData(
      Glyph g = 0,
      const SimpleFontData* f = nullptr,
      CanvasRotationInVertical rotation = CanvasRotationInVertical::kRegular)
      : glyph(g), font_data(f), canvas_rotation(rotation) {}
  Glyph glyph;
  const SimpleFontData* font_data;
  CanvasRotationInVertical canvas_rotation;
};

class FontDescription;

class PLATFORM_EXPORT SimpleFontData : public FontData {
 public:
  // Used to create platform fonts.
  static scoped_refptr<SimpleFontData> Create(
      const FontPlatformData& platform_data,
      scoped_refptr<CustomFontData> custom_data = nullptr,
      bool subpixel_ascent_descent = false) {
    return base::AdoptRef(new SimpleFontData(
        platform_data, std::move(custom_data), subpixel_ascent_descent));
  }

  const FontPlatformData& PlatformData() const { return platform_data_; }

  scoped_refptr<SimpleFontData> SmallCapsFontData(const FontDescription&) const;
  scoped_refptr<SimpleFontData> EmphasisMarkFontData(const FontDescription&) const;

  FontMetrics& GetFontMetrics() { return font_metrics_; }
  const FontMetrics& GetFontMetrics() const { return font_metrics_; }
  float SizePerUnit() const {
    return PlatformData().size() /
           (GetFontMetrics().UnitsPerEm() ? GetFontMetrics().UnitsPerEm() : 1);
  }
  float InternalLeading() const {
    return GetFontMetrics().FloatHeight() - PlatformData().size();
  }

  // "em height" metrics.
  // https://drafts.css-houdini.org/font-metrics-api-1/#fontmetrics
  LayoutUnit EmHeightAscent(FontBaseline = kAlphabeticBaseline) const;
  LayoutUnit EmHeightDescent(FontBaseline = kAlphabeticBaseline) const;

  LayoutUnit VerticalPosition(FontVerticalPositionType, FontBaseline) const;

  float MaxCharWidth() const { return max_char_width_; }
  void SetMaxCharWidth(float max_char_width) {
    max_char_width_ = max_char_width;
  }

  float AvgCharWidth() const { return avg_char_width_; }
  void SetAvgCharWidth(float avg_char_width) {
    avg_char_width_ = avg_char_width;
  }

  FloatRect BoundsForGlyph(Glyph) const;
  void BoundsForGlyphs(const Vector<Glyph, 256>&, Vector<SkRect, 256>*) const;
  FloatRect PlatformBoundsForGlyph(Glyph) const;
  float WidthForGlyph(Glyph) const;
  float PlatformWidthForGlyph(Glyph) const;

  float SpaceWidth() const { return space_width_; }
  void SetSpaceWidth(float space_width) { space_width_ = space_width; }

  Glyph SpaceGlyph() const { return space_glyph_; }
  void SetSpaceGlyph(Glyph space_glyph) { space_glyph_ = space_glyph; }
  Glyph ZeroGlyph() const { return zero_glyph_; }
  void SetZeroGlyph(Glyph zero_glyph) { zero_glyph_ = zero_glyph; }

  const SimpleFontData* FontDataForCharacter(UChar32) const override;

  Glyph GlyphForCharacter(UChar32) const;

  bool IsCustomFont() const override { return custom_font_data_.get(); }
  bool IsLoading() const override {
    return custom_font_data_ ? custom_font_data_->IsLoading() : false;
  }
  bool IsLoadingFallback() const override {
    return custom_font_data_ ? custom_font_data_->IsLoadingFallback() : false;
  }
  bool IsSegmented() const override;
  bool ShouldSkipDrawing() const override {
    return custom_font_data_ && custom_font_data_->ShouldSkipDrawing();
  }

  CustomFontData* GetCustomFontData() const { return custom_font_data_.get(); }

  unsigned VisualOverflowInflationForAscent() const {
    return visual_overflow_inflation_for_ascent_;
  }
  unsigned VisualOverflowInflationForDescent() const {
    return visual_overflow_inflation_for_descent_;
  }

 protected:
  SimpleFontData(const FontPlatformData&,
                 scoped_refptr<CustomFontData> custom_data,
                 bool subpixel_ascent_descent = false);

 private:
  void PlatformInit(bool subpixel_ascent_descent);
  void PlatformGlyphInit();

  scoped_refptr<SimpleFontData> CreateScaledFontData(const FontDescription&,
                                              float scale_factor) const;

  void ComputeEmHeightMetrics() const;
  bool NormalizeEmHeightMetrics(float, float) const;

  FontMetrics font_metrics_;
  float max_char_width_;
  float avg_char_width_;

  FontPlatformData platform_data_;
  SkFont font_;

  Glyph space_glyph_;
  float space_width_;
  Glyph zero_glyph_;

  struct DerivedFontData {
    USING_FAST_MALLOC(DerivedFontData);

   public:
    DerivedFontData() = default;

    scoped_refptr<SimpleFontData> small_caps;
    scoped_refptr<SimpleFontData> emphasis_mark;

    DISALLOW_COPY_AND_ASSIGN(DerivedFontData);
  };

  mutable std::unique_ptr<DerivedFontData> derived_font_data_;

  scoped_refptr<CustomFontData> custom_font_data_;

  // These are set to non-zero when ascent or descent is rounded or shifted
  // to be smaller than the actual ascent or descent. When calculating visual
  // overflows, we should add the inflations.
  unsigned visual_overflow_inflation_for_ascent_;
  unsigned visual_overflow_inflation_for_descent_;

  mutable LayoutUnit em_height_ascent_;
  mutable LayoutUnit em_height_descent_;

// See discussion on crbug.com/631032 and Skiaissue
// https://bugs.chromium.org/p/skia/issues/detail?id=5328 :
// On Mac we're still using path based glyph metrics, and they seem to be
// too slow to be able to remove the caching layer we have here.
#if defined(OS_MACOSX)
  mutable std::unique_ptr<GlyphMetricsMap<FloatRect>> glyph_to_bounds_map_;
  mutable GlyphMetricsMap<float> glyph_to_width_map_;
#endif
};

ALWAYS_INLINE FloatRect SimpleFontData::BoundsForGlyph(Glyph glyph) const {
#if !defined(OS_MACOSX)
  return PlatformBoundsForGlyph(glyph);
#else
  FloatRect bounds_result;
  if (glyph_to_bounds_map_) {
    bounds_result = glyph_to_bounds_map_->MetricsForGlyph(glyph);
    if (bounds_result.Width() != kCGlyphSizeUnknown)
      return bounds_result;
  }

  bounds_result = PlatformBoundsForGlyph(glyph);
  if (!glyph_to_bounds_map_)
    glyph_to_bounds_map_ = std::make_unique<GlyphMetricsMap<FloatRect>>();
  glyph_to_bounds_map_->SetMetricsForGlyph(glyph, bounds_result);

  return bounds_result;
#endif
}

ALWAYS_INLINE float SimpleFontData::WidthForGlyph(Glyph glyph) const {
#if !defined(OS_MACOSX)
  return PlatformWidthForGlyph(glyph);
#else
  float width = glyph_to_width_map_.MetricsForGlyph(glyph);
  if (width != kCGlyphSizeUnknown)
    return width;

  width = PlatformWidthForGlyph(glyph);

  glyph_to_width_map_.SetMetricsForGlyph(glyph, width);
  return width;
#endif
}

DEFINE_FONT_DATA_TYPE_CASTS(SimpleFontData, false);

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SIMPLE_FONT_DATA_H_
