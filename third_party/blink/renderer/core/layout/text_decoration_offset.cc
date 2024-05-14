// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/text_decoration_offset.h"

#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/text_decoration_info.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

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

std::optional<int> ComputeUnderlineOffsetFromFont(
    const blink::FontMetrics& font_metrics,
    float text_underline_offset) {
  if (!font_metrics.UnderlinePosition()) {
    return std::nullopt;
  }

  return roundf(font_metrics.FloatAscent() + *font_metrics.UnderlinePosition() +
                text_underline_offset);
}

}  // namespace

int TextDecorationOffset::ComputeUnderlineOffsetForUnder(
    const Length& style_underline_offset,
    float computed_font_size,
    const SimpleFontData* font_data,
    float text_decoration_thickness,
    FontVerticalPositionType position_type) const {
  const ComputedStyle& style = text_style_;
  FontBaseline baseline_type = style.GetFontBaseline();

  LayoutUnit style_underline_offset_pixels = LayoutUnit::FromFloatRound(
      StyleUnderlineOffsetToPixels(style_underline_offset, computed_font_size));
  if (IsLineOverSide(position_type))
    style_underline_offset_pixels = -style_underline_offset_pixels;

  if (!font_data)
    return 0;
  const LayoutUnit offset =
      LayoutUnit::FromFloatRound(
          font_data->GetFontMetrics().FloatAscent(baseline_type)) -
      font_data->VerticalPosition(position_type, baseline_type) +
      style_underline_offset_pixels;

  // Compute offset to the farthest position of the decorating box.
  // TODO(layout-dev): This does not take farthest offset within the decorating
  // box into account, only the position within this text fragment.
  int offset_int = offset.Floor();

  // Gaps are not needed for TextTop because it generally has internal
  // leadings. Overline needs to grow upwards, hence subtract thickness.
  if (position_type == FontVerticalPositionType::TextTop)
    return offset_int - floorf(text_decoration_thickness);
  return !IsLineOverSide(position_type)
             ? offset_int + 1
             : offset_int - 1 - floorf(text_decoration_thickness);
}

int TextDecorationOffset::ComputeUnderlineOffset(
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
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case ResolvedUnderlinePosition::kNearAlphabeticBaselineFromFont:
      return ComputeUnderlineOffsetFromFont(font_metrics,
                                            style_underline_offset_pixels)
          .value_or(ComputeUnderlineOffsetAuto(
              font_metrics, style_underline_offset_pixels,
              text_decoration_thickness, style_underline_offset.IsFixed()));
    case ResolvedUnderlinePosition::kNearAlphabeticBaselineAuto:
      return ComputeUnderlineOffsetAuto(
          font_metrics, style_underline_offset_pixels,
          text_decoration_thickness, style_underline_offset.IsFixed());
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
float TextDecorationOffset::StyleUnderlineOffsetToPixels(
    const Length& style_underline_offset,
    float font_size) {
  if (style_underline_offset.IsAuto()) {
    return 0;
  }
  return FloatValueForLength(style_underline_offset, font_size);
}

}  // namespace blink
