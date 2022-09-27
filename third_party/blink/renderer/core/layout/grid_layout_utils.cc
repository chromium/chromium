// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_layout_utils.h"

#include "third_party/blink/renderer/core/layout/layout_grid.h"

namespace blink {

static inline bool MarginStartIsAuto(const LayoutBox& child,
                                     GridTrackSizingDirection direction) {
  return direction == kForColumns ? child.StyleRef().MarginStart().IsAuto()
                                  : child.StyleRef().MarginBefore().IsAuto();
}

static inline bool MarginEndIsAuto(const LayoutBox& child,
                                   GridTrackSizingDirection direction) {
  return direction == kForColumns ? child.StyleRef().MarginEnd().IsAuto()
                                  : child.StyleRef().MarginAfter().IsAuto();
}

static bool ChildHasMargin(const LayoutBox& child,
                           GridTrackSizingDirection direction) {
  // Length::IsZero returns true for 'auto' margins, which is aligned with the
  // purpose of this function.
  if (direction == kForColumns) {
    return !child.StyleRef().MarginStart().IsZero() ||
           !child.StyleRef().MarginEnd().IsZero();
  }
  return !child.StyleRef().MarginBefore().IsZero() ||
         !child.StyleRef().MarginAfter().IsZero();
}

static LayoutUnit ComputeMarginLogicalSizeForChild(
    const LayoutGrid& grid,
    MarginDirection for_direction,
    const LayoutBox& child) {
  bool is_inline_direction = for_direction == kInlineDirection;
  GridTrackSizingDirection direction =
      is_inline_direction ? kForColumns : kForRows;
  if (!ChildHasMargin(child, direction))
    return LayoutUnit();

  LayoutUnit margin_start;
  LayoutUnit margin_end;
  LayoutUnit logical_size =
      is_inline_direction ? child.LogicalWidth() : child.LogicalHeight();
  const Length& margin_start_length = is_inline_direction
                                          ? child.StyleRef().MarginStart()
                                          : child.StyleRef().MarginBefore();
  const Length& margin_end_length = is_inline_direction
                                        ? child.StyleRef().MarginEnd()
                                        : child.StyleRef().MarginAfter();
  child.ComputeMarginsForDirection(
      for_direction, &grid, child.ContainingBlockLogicalWidthForContent(),
      logical_size, margin_start, margin_end, margin_start_length,
      margin_end_length);

  return MarginStartIsAuto(child, direction)
             ? margin_end
             : MarginEndIsAuto(child, direction) ? margin_start
                                                 : margin_start + margin_end;
}

LayoutUnit GridLayoutUtils::MarginLogicalWidthForChild(const LayoutGrid& grid,
                                                       const LayoutBox& child) {
  if (child.NeedsLayout())
    return ComputeMarginLogicalSizeForChild(grid, kInlineDirection, child);
  // TODO(rego): Evaluate the possibility of using
  // LayoutBlock::MarginIntrinsicLogicalWidthForChild() (note that this is
  // protected so it cannot be directly used right now) or some similar method
  // for this case.
  LayoutUnit margin_start = child.StyleRef().MarginStart().IsAuto()
                                ? LayoutUnit()
                                : child.MarginStart();
  LayoutUnit margin_end =
      child.StyleRef().MarginEnd().IsAuto() ? LayoutUnit() : child.MarginEnd();
  return margin_start + margin_end;
}

LayoutUnit GridLayoutUtils::MarginLogicalHeightForChild(
    const LayoutGrid& grid,
    const LayoutBox& child) {
  if (child.NeedsLayout())
    return ComputeMarginLogicalSizeForChild(grid, kBlockDirection, child);
  LayoutUnit margin_before = child.StyleRef().MarginBefore().IsAuto()
                                 ? LayoutUnit()
                                 : child.MarginBefore();
  LayoutUnit margin_after = child.StyleRef().MarginAfter().IsAuto()
                                ? LayoutUnit()
                                : child.MarginAfter();
  return margin_before + margin_after;
}

bool GridLayoutUtils::IsOrthogonalChild(const LayoutGrid& grid,
                                        const LayoutBox& child) {
  return child.IsHorizontalWritingMode() != grid.IsHorizontalWritingMode();
}

GridTrackSizingDirection GridLayoutUtils::FlowAwareDirectionForChild(
    const LayoutGrid& grid,
    const LayoutBox& child,
    GridTrackSizingDirection direction) {
  return !IsOrthogonalChild(grid, child)
             ? direction
             : (direction == kForColumns ? kForRows : kForColumns);
}

bool GridLayoutUtils::HasOverrideContainingBlockContentSizeForChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) {
  return direction == kForColumns
             ? child.HasOverrideContainingBlockContentLogicalWidth()
             : child.HasOverrideContainingBlockContentLogicalHeight();
}

LayoutUnit GridLayoutUtils::OverrideContainingBlockContentSizeForChild(
    const LayoutBox& child,
    GridTrackSizingDirection direction) {
  return direction == kForColumns
             ? child.OverrideContainingBlockContentLogicalWidth()
             : child.OverrideContainingBlockContentLogicalHeight();
}

}  // namespace blink
