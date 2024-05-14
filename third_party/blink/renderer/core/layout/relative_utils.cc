// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/relative_utils.h"

#include <optional>

#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

LogicalOffset ComputeRelativeOffset(
    const ComputedStyle& child_style,
    WritingDirectionMode container_writing_direction,
    const LogicalSize& available_size) {
  if (child_style.GetPosition() != EPosition::kRelative)
    return LogicalOffset();

  const PhysicalSize physical_size = ToPhysicalSize(
      available_size, container_writing_direction.GetWritingMode());

  // Helper function to correctly resolve insets.
  auto ResolveInset = [](const Length& length,
                         LayoutUnit size) -> std::optional<LayoutUnit> {
    if (length.IsAuto())
      return std::nullopt;
    if (length.HasPercent() && size == kIndefiniteSize) {
      return std::nullopt;
    }
    return MinimumValueForLength(length, size);
  };

  std::optional<LayoutUnit> left =
      ResolveInset(child_style.Left(), physical_size.width);
  std::optional<LayoutUnit> right =
      ResolveInset(child_style.Right(), physical_size.width);
  std::optional<LayoutUnit> top =
      ResolveInset(child_style.Top(), physical_size.height);
  std::optional<LayoutUnit> bottom =
      ResolveInset(child_style.Bottom(), physical_size.height);

  // Common case optimization.
  if (!left && !right && !top && !bottom)
    return LogicalOffset();

  // Conflict resolution rules: https://www.w3.org/TR/css-position-3/#rel-pos
  if (!left && !right) {
    left = LayoutUnit();
    right = LayoutUnit();
  } else if (!left) {
    left = -*right;
  } else if (!right) {
    right = -*left;
  }

  if (!top && !bottom) {
    top = LayoutUnit();
    bottom = LayoutUnit();
  } else if (!top) {
    top = -*bottom;
  } else if (!bottom) {
    bottom = -*top;
  }

  switch (container_writing_direction.GetWritingMode()) {
    case WritingMode::kHorizontalTb:
      return container_writing_direction.IsLtr() ? LogicalOffset(*left, *top)
                                                 : LogicalOffset(*right, *top);
    case WritingMode::kVerticalRl:
    case WritingMode::kSidewaysRl:
      return container_writing_direction.IsLtr()
                 ? LogicalOffset(*top, *right)
                 : LogicalOffset(*bottom, *right);
    case WritingMode::kVerticalLr:
      return container_writing_direction.IsLtr()
                 ? LogicalOffset(*top, *left)
                 : LogicalOffset(*bottom, *left);
    case WritingMode::kSidewaysLr:
      return container_writing_direction.IsLtr() ? LogicalOffset(*bottom, *left)
                                                 : LogicalOffset(*top, *left);
    default:
      NOTREACHED_IN_MIGRATION();
      return LogicalOffset();
  }
}

LogicalOffset ComputeRelativeOffsetForBoxFragment(
    const PhysicalBoxFragment& fragment,
    WritingDirectionMode container_writing_direction,
    const LogicalSize& available_size) {
  const auto& child_style = fragment.Style();
  DCHECK_EQ(child_style.GetPosition(), EPosition::kRelative);

  return ComputeRelativeOffset(child_style, container_writing_direction,
                               available_size);
}

LogicalOffset ComputeRelativeOffsetForInline(const ConstraintSpace& space,
                                             const ComputedStyle& child_style) {
  if (child_style.GetPosition() != EPosition::kRelative)
    return LogicalOffset();

  // The confliction resolution rules work based off the block's writing-mode
  // and direction, not the child's container. E.g.
  // <span style="direction: rtl;">
  //   <span style="position: relative; left: 100px; right: -50px;"></span>
  // </span>
  // In the above example "left" wins.
  const WritingDirectionMode writing_direction = space.GetWritingDirection();
  LogicalOffset relative_offset = ComputeRelativeOffset(
      child_style, writing_direction, space.AvailableSize());

  // Lines are built in a line-logical coordinate system:
  // https://drafts.csswg.org/css-writing-modes-3/#line-directions
  // Reverse the offset direction if we are in a RTL, or flipped writing-mode.
  if (writing_direction.IsRtl())
    relative_offset.inline_offset = -relative_offset.inline_offset;
  if (writing_direction.IsFlippedLines())
    relative_offset.block_offset = -relative_offset.block_offset;

  return relative_offset;
}

LogicalOffset ComputeRelativeOffsetForOOFInInline(
    const ConstraintSpace& space,
    const ComputedStyle& child_style) {
  if (child_style.GetPosition() != EPosition::kRelative)
    return LogicalOffset();

  // The confliction resolution rules work based off the block's writing-mode
  // and direction, not the child's container. E.g.
  // <span style="direction: rtl;">
  //   <span style="position: relative; left: 100px; right: -50px;"></span>
  // </span>
  // In the above example "left" wins.
  const WritingDirectionMode writing_direction = space.GetWritingDirection();
  LogicalOffset relative_offset = ComputeRelativeOffset(
      child_style, writing_direction, space.AvailableSize());

  // Lines are built in a line-logical coordinate system:
  // https://drafts.csswg.org/css-writing-modes-3/#line-directions
  // Reverse the offset direction if we are in a RTL. We skip adjusting for
  // flipped writing-mode when applying the relative position to an OOF
  // positioned element.
  if (writing_direction.IsRtl())
    relative_offset.inline_offset = -relative_offset.inline_offset;

  return relative_offset;
}

}  // namespace blink
