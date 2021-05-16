// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style_ license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/text_decoration_offset_base.h"

#include <algorithm>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/paint/text_decoration_info.h"
#include "third_party/blink/renderer/platform/fonts/font_metrics.h"
#include "third_party/blink/renderer/platform/fonts/font_vertical_position_type.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace {

int ComputeUnderlineOffsetAuto(const blink::FontMetrics& font_metrics,
                               float text_underline_offset,
                               float text_decoration_thickness) {
  // Compute the gap between the font and the underline. Use at least one
  // pixel gap, if underline is thick then use a bigger gap.
  // Underline position of zero means draw underline on Baseline Position,
  // in Blink we need at least 1-pixel gap to adding following check.
  // Positive underline Position means underline should be drawn above baseline
  // and negative value means drawing below baseline, negating the value as in
  // Blink downward Y-increases.
  int gap = std::max<int>(1, ceilf(text_decoration_thickness / 2.f));

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
    const FontMetrics& font_metrics,
    const Length& style_underline_offset,
    float text_decoration_thickness) const {
  float style_underline_offset_pixels =
      StyleUnderlineOffsetToPixels(style_underline_offset, computed_font_size);
  switch (underline_position) {
    default:
      NOTREACHED();
      FALLTHROUGH;
    case ResolvedUnderlinePosition::kNearAlphabeticBaselineFromFont:
      DCHECK(RuntimeEnabledFeatures::UnderlineOffsetThicknessEnabled());
      return ComputeUnderlineOffsetFromFont(font_metrics,
                                            style_underline_offset_pixels)
          .value_or(ComputeUnderlineOffsetAuto(font_metrics,
                                               style_underline_offset_pixels,
                                               text_decoration_thickness));
    case ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto:
      return ComputeUnderlineOffsetAuto(font_metrics,
                                        style_underline_offset_pixels,
                                        text_decoration_thickness);
    case ResolvedUnderlinePosition::kUnder:
      // Position underline at the under edge of the lowest element's
      // content box.
      return ComputeUnderlineOffsetForUnder(
          style_underline_offset, computed_font_size, text_decoration_thickness,
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
