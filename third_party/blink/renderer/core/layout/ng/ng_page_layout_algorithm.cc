// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_page_layout_algorithm.h"

#include <algorithm>
#include "third_party/blink/renderer/core/layout/ng/inline/ng_baseline.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

NGPageLayoutAlgorithm::NGPageLayoutAlgorithm(NGBlockNode node,
                                             const NGConstraintSpace& space,
                                             const NGBreakToken* break_token)
    : NGLayoutAlgorithm(node, space, ToNGBlockBreakToken(break_token)) {}

scoped_refptr<NGLayoutResult> NGPageLayoutAlgorithm::Layout() {
  NGBoxStrut borders = ComputeBorders(ConstraintSpace(), Node());
  NGBoxStrut scrollbars = Node().GetScrollbarSizes();
  NGBoxStrut padding = ComputePadding(ConstraintSpace(), Style());
  NGBoxStrut border_scrollbar_padding = borders + scrollbars + padding;
  NGLogicalSize border_box_size =
      CalculateBorderBoxSize(ConstraintSpace(), Node());
  NGLogicalSize content_box_size =
      ShrinkAvailableSize(border_box_size, border_scrollbar_padding);
  NGLogicalSize page_size = content_box_size;

  NGConstraintSpace child_space = CreateConstraintSpaceForPages(page_size);
  container_builder_.SetInlineSize(border_box_size.inline_size);

  WritingMode writing_mode = ConstraintSpace().GetWritingMode();
  scoped_refptr<const NGBlockBreakToken> break_token = BreakToken();
  LayoutUnit intrinsic_block_size;
  NGLogicalOffset page_offset(border_scrollbar_padding.StartOffset());
  NGLogicalOffset page_progression;
  if (Style().OverflowY() == EOverflow::kWebkitPagedX) {
    page_progression.inline_offset = page_size.inline_size;
  } else {
    // TODO(mstensho): Handle auto block size.
    page_progression.block_offset = page_size.block_size;
  }

  do {
    // Lay out one page. Each page will become a fragment.
    NGBlockLayoutAlgorithm child_algorithm(Node(), child_space,
                                           break_token.get());
    scoped_refptr<NGLayoutResult> result = child_algorithm.Layout();
    scoped_refptr<const NGPhysicalBoxFragment> page(
        ToNGPhysicalBoxFragment(result->PhysicalFragment().get()));

    container_builder_.AddChild(*result, page_offset);

    NGBoxFragment logical_fragment(writing_mode, ConstraintSpace().Direction(),
                                   *page);
    intrinsic_block_size =
        std::max(intrinsic_block_size,
                 page_offset.block_offset + logical_fragment.BlockSize());
    page_offset += page_progression;
    break_token = ToNGBlockBreakToken(page->BreakToken());
  } while (break_token && !break_token->IsFinished());

  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);

  // Recompute the block-axis size now that we know our content size.
  border_box_size.block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), intrinsic_block_size);
  container_builder_.SetBlockSize(border_box_size.block_size);
  container_builder_.SetBorders(ComputeBorders(ConstraintSpace(), Style()));
  container_builder_.SetPadding(ComputePadding(ConstraintSpace(), Style()));

  NGOutOfFlowLayoutPart(&container_builder_, Node().IsAbsoluteContainer(),
                        Node().IsFixedContainer(), borders + scrollbars,
                        ConstraintSpace(), Style())
      .Run();

  // TODO(mstensho): Propagate baselines.

  return container_builder_.ToBoxFragment();
}

base::Optional<MinMaxSize> NGPageLayoutAlgorithm::ComputeMinMaxSize(
    const MinMaxSizeInput& input) const {
  NGBlockLayoutAlgorithm algorithm(Node(), ConstraintSpace());
  return algorithm.ComputeMinMaxSize(input);
}

NGConstraintSpace NGPageLayoutAlgorithm::CreateConstraintSpaceForPages(
    const NGLogicalSize& page_size) const {
  NGConstraintSpaceBuilder space_builder(ConstraintSpace());
  space_builder.SetAvailableSize(page_size);
  space_builder.SetPercentageResolutionSize(page_size);

  if (NGBaseline::ShouldPropagateBaselines(Node()))
    space_builder.AddBaselineRequests(ConstraintSpace().BaselineRequests());

  // TODO(mstensho): Handle auto block size.
  space_builder.SetFragmentationType(kFragmentPage);
  space_builder.SetFragmentainerBlockSize(page_size.block_size);
  space_builder.SetFragmentainerSpaceAtBfcStart(page_size.block_size);
  space_builder.SetIsNewFormattingContext(true);
  space_builder.SetIsAnonymous(true);

  return space_builder.ToConstraintSpace(Style().GetWritingMode());
}

}  // namespace blink
