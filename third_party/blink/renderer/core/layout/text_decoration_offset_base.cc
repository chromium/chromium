// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/text_decoration_offset_base.h"

#include <algorithm>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/paint/text_decoration_info.h"
#include "third_party/blink/renderer/platform/fonts/font_metrics.h"
#include "third_party/blink/renderer/platform/fonts/font_vertical_position_type.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace {

int ComputeUnderlineOffsetAuto(const blink::FontMetrics& font_metrics,
                               float text_underline_offset,
                               float text_decoration_thickness,
                               bool is_fixed) {
  // Compute the gap between the font and the underline.
  // Underline position of zero means draw underline on Baseline Position.
  // When text-underline-offset is a fixed length, the gap should be zero.
  // If it is not a fixed length, use at least one
  // pixel gap. If underline is thick then use a bigger gap.
  // Positive underline Position means underline should be drawn below baseline
  // and negative value means drawing above baseline.
  int gap{is_fixed ? 0
                   : std::max<int>(1, ceilf(text_decoration_thickness / 2.f))};

  // Position underline near the alphabetic baseline.
  return font_metrics.Ascent() + gap + roundf(text_underline_offset);
}

absl::optional<int> ComputeUnderlineOffsetFromFont(
    const blink::FontMetrics& font_metrics,
    float text_underline_offset) {
  if (!font_metrics.UnderlinePosition())
    return absl::nullopt;

  return roundf(font_metrics.FloatAscent() + *font_metrics.UnderlinePosition() +
                text_underline_offset);
}

}  // namespace

namespace blink {

int TextDecorationOffsetBase::ComputeUnderlineOffset(
    ResolvedUnderlinePosition underline_position,
    float computed_font_size,
    const SimpleFontData* font_data,
    const Length& style_underline_offset,
    float text_decoration_thickness) const {
  float style_underline_offset_pixels =
      StyleUnderlineOffsetToPixels(style_underline_offset, computed_font_size);

  const FontMetrics& font_metrics = font_data->GetFontMetrics();

  switch (underline_position) {
    default:
      NOTREACHED();
      [[fallthrough]];
    case ResolvedUnderlinePosition::kNearAlphabeticBaselineFromFont:
      return ComputeUnderlineOffsetFromFont(font_metrics,
                                            style_underline_offset_pixels)
          .value_or(ComputeUnderlineOffsetAuto(font_metrics,
                                               style_underline_offset_pixels,
                                               text_decoration_thickness,
                                               style_underline_offset.IsFixed()));
    case ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto:
      return ComputeUnderlineOffsetAuto(font_metrics,
                                        style_underline_offset_pixels,
                                        text_decoration_thickness,
                                        style_underline_offset.IsFixed());
    case ResolvedUnderlinePosition::kUnder:
      // Position underline at the under edge of the lowest element's
      // content box.
      return ComputeUnderlineOffsetForUnder(
          style_underline_offset, computed_font_size, font_data,
          text_decoration_thickness,
          FontVerticalPositionType::BottomOfEmHeight);
  }
}

/* static */
float TextDecorationOffsetBase::StyleUnderlineOffsetToPixels(
    const Length& style_underline_offset,
    float font_size) {
  if (style_underline_offset.IsAuto())
    return 0;
  return FloatValueForLength(style_underline_offset, font_size);
}

}  // namespace blink
