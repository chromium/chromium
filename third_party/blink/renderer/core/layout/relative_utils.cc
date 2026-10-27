// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/relative_utils.h"

#include <optional>

#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/geometry/physical_offset.h"
#include "third_party/blink/renderer/platform/geometry/physical_size.h"

namespace blink {

PhysicalBoxStrut ComputeRelativeInsets(
    const ComputedStyle& child_style,
    WritingDirectionMode container_writing_direction,
    const LogicalSize& available_size) {
  if (child_style.GetPosition() != EPosition::kRelative) {
    return PhysicalBoxStrut();
  }

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

  return PhysicalBoxStrut(*top, *right, *bottom, *left);
}

PhysicalBoxStrut ComputeRelativeInsetsForBoxFragment(
    const PhysicalBoxFragment& fragment,
    WritingDirectionMode container_writing_direction,
    const LogicalSize& available_size) {
  const auto& child_style = fragment.Style();
  DCHECK_EQ(child_style.GetPosition(), EPosition::kRelative);

  return ComputeRelativeInsets(child_style, container_writing_direction,
                               available_size);
}

LogicalOffset ComputeRelativeOffsetForInline(
    const PhysicalBoxStrut& relative_insets,
    WritingDirectionMode container_writing_direction) {
  LogicalOffset relative_offset =
      relative_insets.ConvertToLogical(container_writing_direction)
          .StartOffset();

  // Lines are built in a line-logical coordinate system:
  // https://drafts.csswg.org/css-writing-modes-3/#line-directions
  // Reverse the offset direction if we are in a RTL, or flipped writing-mode.
  if (container_writing_direction.IsRtl()) {
    relative_offset.inline_offset = -relative_offset.inline_offset;
  }
  if (container_writing_direction.IsFlippedLines()) {
    relative_offset.block_offset = -relative_offset.block_offset;
  }

  return relative_offset;
}

LogicalOffset ComputeRelativeOffsetForOOFInInline(
    const PhysicalBoxStrut& relative_insets,
    WritingDirectionMode container_writing_direction) {
  LogicalOffset relative_offset =
      relative_insets.ConvertToLogical(container_writing_direction)
          .StartOffset();

  // Lines are built in a line-logical coordinate system:
  // https://drafts.csswg.org/css-writing-modes-3/#line-directions
  // Reverse the offset direction if we are in a RTL. We skip adjusting for
  // flipped writing-mode when applying the relative position to an OOF
  // positioned element.
  if (container_writing_direction.IsRtl()) {
    relative_offset.inline_offset = -relative_offset.inline_offset;
  }

  return relative_offset;
}

}  // namespace blink
