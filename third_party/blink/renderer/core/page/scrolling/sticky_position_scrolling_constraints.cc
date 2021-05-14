// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/sticky_position_scrolling_constraints.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

PhysicalOffset StickyPositionScrollingConstraints::ComputeStickyOffset(
    const PhysicalRect& content_box_rect,
    const StickyConstraintsMap& constraints_map) {
  PhysicalRect sticky_box_rect = scroll_container_relative_sticky_box_rect;
  PhysicalRect containing_block_rect =
      scroll_container_relative_containing_block_rect;
  PhysicalOffset ancestor_sticky_box_offset =
      AncestorStickyBoxOffset(constraints_map);
  PhysicalOffset ancestor_containing_block_offset =
      AncestorContainingBlockOffset(constraints_map);

  // Adjust the cached rect locations for any sticky ancestor elements. The
  // sticky offset applied to those ancestors affects us as follows:
  //
  //   1. |nearest_sticky_layer_shifting_sticky_box| is a sticky layer between
  //      ourselves and our containing block, e.g. a nested inline parent.
  //      It shifts only the sticky_box_rect and not the containing_block_rect.
  //   2. |nearest_sticky_layer_shifting_containing_block| is a sticky layer
  //      between our containing block (inclusive) and our scroll ancestor
  //      (exclusive). As such, it shifts both the sticky_box_rect and the
  //      containing_block_rect.
  //
  // Note that this calculation assumes that |ComputeStickyOffset| is being
  // called top down, e.g. it has been called on any ancestors we have before
  // being called on us.
  sticky_box_rect.Move(ancestor_sticky_box_offset +
                       ancestor_containing_block_offset);
  containing_block_rect.Move(ancestor_containing_block_offset);

  // We now attempt to shift sticky_box_rect to obey the specified sticky
  // constraints, whilst always staying within our containing block. This
  // shifting produces the final sticky offset below.
  //
  // As per the spec, 'left' overrides 'right' and 'top' overrides 'bottom'.
  PhysicalRect box_rect = sticky_box_rect;

  if (is_anchored_right) {
    LayoutUnit right_limit = content_box_rect.Right() - right_offset;
    LayoutUnit right_delta = right_limit - sticky_box_rect.Right();
    LayoutUnit available_space =
        containing_block_rect.X() - sticky_box_rect.X();

    right_delta = right_delta.ClampPositiveToZero();
    available_space = available_space.ClampPositiveToZero();

    if (right_delta < available_space)
      right_delta = available_space;

    box_rect.Move(PhysicalOffset(right_delta, LayoutUnit()));
  }

  if (is_anchored_left) {
    LayoutUnit left_limit = content_box_rect.X() + left_offset;
    LayoutUnit left_delta = left_limit - sticky_box_rect.X();
    LayoutUnit available_space =
        containing_block_rect.Right() - sticky_box_rect.Right();

    left_delta = left_delta.ClampNegativeToZero();
    available_space = available_space.ClampNegativeToZero();

    if (left_delta > available_space)
      left_delta = available_space;

    box_rect.Move(PhysicalOffset(left_delta, LayoutUnit()));
  }

  if (is_anchored_bottom) {
    LayoutUnit bottom_limit = content_box_rect.Bottom() - bottom_offset;
    LayoutUnit bottom_delta = bottom_limit - sticky_box_rect.Bottom();
    LayoutUnit available_space =
        containing_block_rect.Y() - sticky_box_rect.Y();

    bottom_delta = bottom_delta.ClampPositiveToZero();
    available_space = available_space.ClampPositiveToZero();

    if (bottom_delta < available_space)
      bottom_delta = available_space;

    box_rect.Move(PhysicalOffset(LayoutUnit(), bottom_delta));
  }

  if (is_anchored_top) {
    LayoutUnit top_limit = content_box_rect.Y() + top_offset;
    LayoutUnit top_delta = top_limit - sticky_box_rect.Y();
    LayoutUnit available_space =
        containing_block_rect.Bottom() - sticky_box_rect.Bottom();

    top_delta = top_delta.ClampNegativeToZero();
    available_space = available_space.ClampNegativeToZero();

    if (top_delta > available_space)
      top_delta = available_space;

    box_rect.Move(PhysicalOffset(LayoutUnit(), top_delta));
  }

  PhysicalOffset sticky_offset = box_rect.offset - sticky_box_rect.offset;

  // Now that we have computed our current sticky offset, update the cached
  // accumulated sticky offsets.
  total_sticky_box_sticky_offset = ancestor_sticky_box_offset + sticky_offset;
  total_containing_block_sticky_offset = ancestor_sticky_box_offset +
                                         ancestor_containing_block_offset +
                                         sticky_offset;

  return sticky_offset;
}

PhysicalOffset StickyPositionScrollingConstraints::GetOffsetForStickyPosition(
    const StickyConstraintsMap& constraints_map) const {
  return total_sticky_box_sticky_offset -
         AncestorStickyBoxOffset(constraints_map);
}

PhysicalOffset StickyPositionScrollingConstraints::AncestorStickyBoxOffset(
    const StickyConstraintsMap& constraints_map) const {
  if (!nearest_sticky_layer_shifting_sticky_box)
    return PhysicalOffset();
  DCHECK(constraints_map.Contains(nearest_sticky_layer_shifting_sticky_box));
  return constraints_map.at(nearest_sticky_layer_shifting_sticky_box)
      .total_sticky_box_sticky_offset;
}

PhysicalOffset
StickyPositionScrollingConstraints::AncestorContainingBlockOffset(
    const StickyConstraintsMap& constraints_map) const {
  if (!nearest_sticky_layer_shifting_containing_block) {
    return PhysicalOffset();
  }
  DCHECK(
      constraints_map.Contains(nearest_sticky_layer_shifting_containing_block));
  return constraints_map.at(nearest_sticky_layer_shifting_containing_block)
      .total_containing_block_sticky_offset;
}

}  // namespace blink
