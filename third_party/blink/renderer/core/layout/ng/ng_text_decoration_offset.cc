// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_text_decoration_offset.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

int NGTextDecorationOffset::ComputeUnderlineOffsetForUnder(
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

}  // namespace blink
