// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_box_utils.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_static_position.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"

namespace blink {

LayoutUnit LayoutBoxUtils::AvailableLogicalWidth(const LayoutBox& box,
                                                 const LayoutBlock* cb) {
  auto writing_mode = box.StyleRef().GetWritingMode();
  bool parallel_containing_block = IsParallelWritingMode(
      cb ? cb->StyleRef().GetWritingMode() : writing_mode, writing_mode);

  // Grid layout sets OverrideContainingBlockContentLogicalWidth|Height
  if (parallel_containing_block &&
      box.HasOverrideContainingBlockContentLogicalWidth()) {
    return box.OverrideContainingBlockContentLogicalWidth()
        .ClampNegativeToZero();
  }

  if (!parallel_containing_block &&
      box.HasOverrideContainingBlockContentLogicalHeight()) {
    return box.OverrideContainingBlockContentLogicalHeight()
        .ClampNegativeToZero();
  }

  if (parallel_containing_block)
    return box.ContainingBlockLogicalWidthForContent().ClampNegativeToZero();

  return box.PerpendicularContainingBlockLogicalHeight().ClampNegativeToZero();
}

LayoutUnit LayoutBoxUtils::AvailableLogicalHeight(const LayoutBox& box,
                                                  const LayoutBlock* cb) {
  auto writing_mode = box.StyleRef().GetWritingMode();
  bool parallel_containing_block = IsParallelWritingMode(
      cb ? cb->StyleRef().GetWritingMode() : writing_mode, writing_mode);

  // Grid layout sets OverrideContainingBlockContentLogicalWidth|Height
  if (parallel_containing_block &&
      box.HasOverrideContainingBlockContentLogicalHeight())
    return box.OverrideContainingBlockContentLogicalHeight();

  if (!parallel_containing_block &&
      box.HasOverrideContainingBlockContentLogicalWidth())
    return box.OverrideContainingBlockContentLogicalWidth();

  if (!box.Parent())
    return box.View()->ViewLogicalHeightForPercentages();

  DCHECK(cb);
  if (parallel_containing_block)
    return box.ContainingBlockLogicalHeightForPercentageResolution();

  return box.ContainingBlockLogicalWidthForContent();
}

NGLogicalStaticPosition LayoutBoxUtils::ComputeStaticPositionFromLegacy(
    const LayoutBox& box,
    const NGBoxStrut& container_border_scrollbar,
    const NGBoxFragmentBuilder* container_builder) {
  const LayoutBoxModelObject* css_container =
      ToLayoutBoxModelObject(box.Container());
  const TextDirection parent_direction = box.Parent()->StyleRef().Direction();

  // These two values represent the available-size for the OOF-positioned
  // descandant, in the *descendant's* writing mode.
  LayoutUnit containing_block_logical_width =
      box.ContainingBlockLogicalWidthForPositioned(css_container);
  LayoutUnit containing_block_logical_height =
      box.ContainingBlockLogicalHeightForPositioned(css_container);

  Length logical_left;
  Length logical_right;
  Length logical_top;
  Length logical_bottom;
  box.ComputeInlineStaticDistance(logical_left, logical_right, &box,
                                  css_container, containing_block_logical_width,
                                  container_builder);
  box.ComputeBlockStaticDistance(logical_top, logical_bottom, &box,
                                 css_container, container_builder);

  // Determine the static-position.
  LayoutUnit static_line;
  LayoutUnit static_block;
  if (IsLtr(parent_direction)) {
    if (!logical_left.IsAuto()) {
      static_line =
          MinimumValueForLength(logical_left, containing_block_logical_width);
    }
  } else {
    if (!logical_right.IsAuto()) {
      static_line =
          MinimumValueForLength(logical_right, containing_block_logical_width);
    }

    // |logical_right| is an adjustment from the right edge, to keep this
    // relative to the line-left edge account for the
    // |containing_block_logical_width|.
    static_line = containing_block_logical_width - static_line;
  }
  if (!logical_top.IsAuto()) {
    static_block =
        MinimumValueForLength(logical_top, containing_block_logical_height);
  }

  NGLogicalStaticPosition logical_static_position{
      {static_line, static_block},
      IsLtr(parent_direction)
          ? NGLogicalStaticPosition::InlineEdge::kInlineStart
          : NGLogicalStaticPosition::InlineEdge::kInlineEnd,
      NGLogicalStaticPosition::BlockEdge::kBlockStart};

  // Determine the physical available-size, remember that the available-size is
  // currently in the *descendant's* writing-mode.
  PhysicalSize container_size =
      ToPhysicalSize(LogicalSize(containing_block_logical_width,
                                 containing_block_logical_height),
                     box.StyleRef().GetWritingMode());

  const LayoutBox* container = css_container->IsBox()
                                   ? ToLayoutBox(css_container)
                                   : box.ContainingBlock();
  const WritingMode container_writing_mode =
      container->StyleRef().GetWritingMode();
  const TextDirection container_direction = container->StyleRef().Direction();

  // We perform a logical-physical-logical conversion to convert the
  // static-position into the correct writing-mode, and direction combination.
  //
  // At the moment the static-position is in line-relative coordinates which is
  // why we use |TextDirection::kLtr| for the first conversion.
  logical_static_position =
      logical_static_position
          .ConvertToPhysical(container_writing_mode, TextDirection::kLtr,
                             container_size)
          .ConvertToLogical(container_writing_mode, container_direction,
                            container_size);

  // Finally we shift the static-position from being relative to the
  // padding-box, to the border-box.
  logical_static_position.offset +=
      LogicalOffset{container_border_scrollbar.inline_start,
                    container_border_scrollbar.block_start};
  return logical_static_position;
}

bool LayoutBoxUtils::SkipContainingBlockForPercentHeightCalculation(
    const LayoutBlock* cb) {
  return LayoutBox::SkipContainingBlockForPercentHeightCalculation(cb);
}

}  // namespace blink
