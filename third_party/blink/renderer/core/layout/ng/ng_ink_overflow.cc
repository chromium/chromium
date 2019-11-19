// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_ink_overflow.h"

#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/line/line_orientation_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

// static
void NGInkOverflow::ComputeTextInkOverflow(
    const NGTextFragmentPaintInfo& text_info,
    const ComputedStyle& style,
    const PhysicalSize& size,
    std::unique_ptr<NGInkOverflow>* ink_overflow_out) {
  // Glyph bounds is in logical coordinate, origin at the alphabetic baseline.
  const Font& font = style.GetFont();
  const FloatRect text_ink_bounds = font.TextInkBounds(text_info);
  LayoutRect ink_overflow = EnclosingLayoutRect(text_ink_bounds);

  // Make the origin at the logical top of this fragment.
  if (const SimpleFontData* font_data = font.PrimaryFont()) {
    ink_overflow.SetY(
        ink_overflow.Y() +
        font_data->GetFontMetrics().FixedAscent(kAlphabeticBaseline));
  }

  if (float stroke_width = style.TextStrokeWidth()) {
    ink_overflow.Inflate(LayoutUnit::FromFloatCeil(stroke_width / 2.0f));
  }

  const WritingMode writing_mode = style.GetWritingMode();
  if (style.GetTextEmphasisMark() != TextEmphasisMark::kNone) {
    LayoutUnit emphasis_mark_height =
        LayoutUnit(font.EmphasisMarkHeight(style.TextEmphasisMarkString()));
    DCHECK_GT(emphasis_mark_height, LayoutUnit());
    if (style.GetTextEmphasisLineLogicalSide() == LineLogicalSide::kOver) {
      ink_overflow.ShiftYEdgeTo(
          std::min(ink_overflow.Y(), -emphasis_mark_height));
    } else {
      LayoutUnit logical_height =
          IsHorizontalWritingMode(writing_mode) ? size.height : size.width;
      ink_overflow.ShiftMaxYEdgeTo(
          std::max(ink_overflow.MaxY(), logical_height + emphasis_mark_height));
    }
  }

  if (ShadowList* text_shadow = style.TextShadow()) {
    LayoutRectOutsets text_shadow_logical_outsets =
        LineOrientationLayoutRectOutsets(
            LayoutRectOutsets(text_shadow->RectOutsetsIncludingOriginal()),
            writing_mode);
    text_shadow_logical_outsets.ClampNegativeToZero();
    ink_overflow.Expand(text_shadow_logical_outsets);
  }

  PhysicalRect local_ink_overflow =
      LogicalRect(ink_overflow).ConvertToPhysical(writing_mode, size);

  // Uniting the frame rect ensures that non-ink spaces such side bearings, or
  // even space characters, are included in the visual rect for decorations.
  PhysicalRect local_rect(PhysicalOffset(), size);
  if (local_rect.Contains(local_ink_overflow)) {
    *ink_overflow_out = nullptr;
    return;
  }
  local_ink_overflow.Unite(local_rect);
  local_ink_overflow.ExpandEdgesToPixelBoundaries();
  if (!*ink_overflow_out) {
    *ink_overflow_out = std::make_unique<NGInkOverflow>(local_ink_overflow);
    return;
  }
  (*ink_overflow_out)->self_ink_overflow = local_ink_overflow;
}

}  // namespace blink
