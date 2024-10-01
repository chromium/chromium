// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/replaced_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

ReplacedLayoutAlgorithm::ReplacedLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
}

const LayoutResult* ReplacedLayoutAlgorithm::Layout() {
  DCHECK(!GetBreakToken() || GetBreakToken()->IsBreakBefore());

  if (Node().IsMedia()) {
    LayoutMediaChildren();
  }

  if (Node().IsCanvas() &&
      RuntimeEnabledFeatures::CanvasPlaceElementEnabled()) {
    LayoutCanvasChildren();
  }

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult ReplacedLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  NOTREACHED_IN_MIGRATION();
  return MinMaxSizesResult();
}

// This is necessary for CanvasRenderingContext2D.placeElement().
void ReplacedLayoutAlgorithm::LayoutCanvasChildren() {
  for (LayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    DCHECK(!child.IsFloating());
    DCHECK(!child.IsOutOfFlowPositioned());

    ConstraintSpaceBuilder space_builder(GetConstraintSpace().GetWritingMode(),
                                         child.Style().GetWritingDirection(),
                                         /* is_new_fc= */ true);

    space_builder.SetAvailableSize(ChildAvailableSize());
    space_builder.SetPercentageResolutionSize(ChildAvailableSize());
    space_builder.SetIsPaintedAtomically(true);

    const LayoutResult* result =
        To<BlockNode>(child).Layout(space_builder.ToConstraintSpace());
    // Since this only works with placeElement(), we ignore relative placement
    // and put the element at (0,0) because it will be placed explicitly by
    // the user.
    container_builder_.AddResult(*result,
                                 LogicalOffset(LayoutUnit(), LayoutUnit()));
  }
}

void ReplacedLayoutAlgorithm::LayoutMediaChildren() {
  WritingModeConverter converter(GetConstraintSpace().GetWritingDirection(),
                                 container_builder_.Size());
  LogicalRect logical_new_rect(
      BorderPadding().StartOffset(),
      ShrinkLogicalSize(container_builder_.Size(), BorderPadding()));
  PhysicalRect new_rect = converter.ToPhysical(logical_new_rect);

  for (LayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    LayoutUnit width = new_rect.Width();
    if (child.GetDOMNode()->IsMediaControls()) {
      width =
          To<LayoutMedia>(Node().GetLayoutBox())->ComputePanelWidth(new_rect);
    }

    ConstraintSpaceBuilder space_builder(GetConstraintSpace().GetWritingMode(),
                                         child.Style().GetWritingDirection(),
                                         /* is_new_fc */ true);
    LogicalSize child_size =
        converter.ToLogical(PhysicalSize(width, new_rect.Height()));
    space_builder.SetAvailableSize(child_size);
    space_builder.SetIsFixedInlineSize(true);
    space_builder.SetIsFixedBlockSize(true);

    const LayoutResult* result =
        To<BlockNode>(child).Layout(space_builder.ToConstraintSpace());
    LogicalOffset offset = converter.ToLogical(
        new_rect.offset, result->GetPhysicalFragment().Size());
    container_builder_.AddResult(*result, offset);
  }
}

}  // namespace blink
