// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_table_box_component.h"

#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

void LayoutTableBoxComponent::InvalidateCollapsedBordersOnStyleChange(
    const LayoutObject& table_part,
    LayoutTable& table,
    const StyleDifference& diff,
    const ComputedStyle& old_style) {
  if (!table.ShouldCollapseBorders())
    return;
  if (!old_style.BorderEquals(table_part.StyleRef()) ||
      (diff.TextDecorationOrColorChanged() &&
       table_part.StyleRef().HasBorderColorReferencingCurrentColor()))
    table.InvalidateCollapsedBorders();
}

bool LayoutTableBoxComponent::DoCellsHaveDirtyWidth(
    const LayoutObject& table_part,
    const LayoutTable& table,
    const StyleDifference& diff,
    const ComputedStyle& old_style) {
  // ComputedStyle::diffNeedsFullLayoutAndPaintInvalidation sets needsFullLayout
  // when border sizes change: checking diff.needsFullLayout() is an
  // optimization, not required for correctness.
  // TODO(dgrogan): Remove tablePart.needsLayout()? Perhaps it was an old
  // optimization but now it seems that diff.needsFullLayout() implies
  // tablePart.needsLayout().
  return diff.NeedsFullLayout() && table_part.NeedsLayout() &&
         table.ShouldCollapseBorders() &&
         !old_style.BorderSizeEquals(*table_part.Style());
}

void LayoutTableBoxComponent::MutableForPainting::UpdatePaintResult(
    PaintResult paint_result,
    const CullRect& paint_rect) {
  // A table row or section may paint large background display item which
  // contains paint operations of the background in each contained cell.
  // The display item can be clipped by the paint rect to avoid painting
  // on areas not interested. If we didn't fully paint and paint rect changes,
  // we need to invalidate the display item (using setDisplayItemUncached()
  // because we are already in painting.)
  auto& box = static_cast<LayoutTableBoxComponent&>(layout_object_);
  if (box.last_paint_result_ != kFullyPainted &&
      box.last_paint_rect_ != paint_rect)
    static_cast<const DisplayItemClient&>(layout_object_).Invalidate();

  box.last_paint_result_ = paint_result;
  box.last_paint_rect_ = paint_rect;
}

}  // namespace blink
