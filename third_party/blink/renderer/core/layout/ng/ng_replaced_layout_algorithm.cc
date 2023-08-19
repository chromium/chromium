// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_replaced_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"

namespace blink {

NGReplacedLayoutAlgorithm::NGReplacedLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
}

const NGLayoutResult* NGReplacedLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken() || BreakToken()->IsBreakBefore());
  // TODO(crbug.com/1252693): kIgnoreBlockLengths applies inline constraints
  // through the aspect ratio. But the aspect ratio is ignored when computing
  // the intrinsic block size for NON-replaced elements. This is inconsistent
  // and could lead to subtle bugs.
  const LayoutUnit intrinsic_block_size =
      ComputeReplacedSize(Node(), ConstraintSpace(), BorderPadding(),
                          /* override_available_size */ absl::nullopt,
                          ReplacedSizeMode::kIgnoreBlockLengths)
          .block_size;
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);

  if (Node().IsMedia()) {
    LayoutMediaChildren();
  }

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGReplacedLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  // Most layouts are interested in the min/max content contribution which will
  // call |ComputeReplacedSize| directly. (Which doesn't invoke the code below).
  // This is only used by flex, which expects inline-lengths to be ignored for
  // the min/max content size.
  MinMaxSizes sizes;
  sizes = ComputeReplacedSize(Node(), ConstraintSpace(), BorderPadding(),
                              /* override_available_size */ absl::nullopt,
                              ReplacedSizeMode::kIgnoreInlineLengths)
              .inline_size;

  const bool depends_on_block_constraints =
      Style().LogicalHeight().IsPercentOrCalc() ||
      Style().LogicalMinHeight().IsPercentOrCalc() ||
      Style().LogicalMaxHeight().IsPercentOrCalc() ||
      (Style().LogicalHeight().IsAuto() &&
       ConstraintSpace().IsBlockAutoBehaviorStretch());
  return {sizes, depends_on_block_constraints};
}

void NGReplacedLayoutAlgorithm::LayoutMediaChildren() {
  WritingModeConverter converter(ConstraintSpace().GetWritingDirection(),
                                 container_builder_.Size());
  LogicalRect logical_new_rect(
      BorderPadding().StartOffset(),
      ShrinkLogicalSize(container_builder_.Size(), BorderPadding()));
  PhysicalRect new_rect = converter.ToPhysical(logical_new_rect);

  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    LayoutUnit width = new_rect.Width();
    if (child.GetDOMNode()->IsMediaControls()) {
      width =
          To<LayoutMedia>(Node().GetLayoutBox())->ComputePanelWidth(new_rect);
    }

    NGConstraintSpaceBuilder space_builder(ConstraintSpace().GetWritingMode(),
                                           child.Style().GetWritingDirection(),
                                           /* is_new_fc */ true);
    LogicalSize child_size = converter.ToLogical({width, new_rect.Height()});
    space_builder.SetAvailableSize(child_size);
    space_builder.SetIsFixedInlineSize(true);
    space_builder.SetIsFixedBlockSize(true);

    const NGLayoutResult* result =
        To<NGBlockNode>(child).Layout(space_builder.ToConstraintSpace());
    LogicalOffset offset =
        converter.ToLogical(new_rect.offset, result->PhysicalFragment().Size());
    container_builder_.AddResult(*result, offset);
  }
}

}  // namespace blink
