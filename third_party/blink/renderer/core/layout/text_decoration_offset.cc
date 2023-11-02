// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/text_decoration_offset.h"

#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/platform/fonts/font_metrics.h"

namespace blink {

int TextDecorationOffset::ComputeUnderlineOffsetForUnder(
    const Length& style_underline_offset,
    float computed_font_size,
    const SimpleFontData*,
    float text_decoration_thickness,
    FontVerticalPositionType position_type) const {
  const RootInlineBox& root = inline_text_box_->Root();
  FontBaseline baseline_type = root.BaselineType();

  LayoutUnit style_underline_offset_pixels = LayoutUnit::FromFloatRound(
      StyleUnderlineOffsetToPixels(style_underline_offset, computed_font_size));
  if (IsLineOverSide(position_type)) {
    style_underline_offset_pixels = -style_underline_offset_pixels;
  }

  LayoutUnit offset = inline_text_box_->OffsetTo(position_type, baseline_type) +
                      style_underline_offset_pixels;

  // Compute offset to the farthest position of the decorating box.
  LayoutUnit logical_top = inline_text_box_->LogicalTop();
  LayoutUnit position = logical_top + offset;
  LayoutUnit farthest = root.FarthestPositionForUnderline(
      decorating_box_, position_type, baseline_type, position);
  // Round() looks more logical but Floor() produces better results in
  // positive/negative offsets, in horizontal/vertical flows, on Win/Mac/Linux.
  int offset_int = (farthest - logical_top).Floor();

  // Gaps are not needed for TextTop because it generally has internal
  // leadings. Overline needs to grow upwards, hence subtract thickness.
  if (position_type == FontVerticalPositionType::TextTop)
    return offset_int - floorf(text_decoration_thickness);
  return !IsLineOverSide(position_type)
             ? offset_int + 1
             : offset_int - 1 - floorf(text_decoration_thickness);
}

}  // namespace blink
