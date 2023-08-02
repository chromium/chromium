// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_page_layout_algorithm.h"

#include <algorithm>
#include "third_party/blink/renderer/core/layout/ng/layout_ng_view.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

NGPageLayoutAlgorithm::NGPageLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {}

const NGLayoutResult* NGPageLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken());
  WritingDirectionMode writing_direction =
      ConstraintSpace().GetWritingDirection();
  const NGBlockBreakToken* break_token = nullptr;
  LayoutUnit intrinsic_block_size;
  LogicalOffset page_offset;
  uint32_t page_index = 0;
  AtomicString page_name;

  container_builder_.SetIsBlockFragmentationContextRoot();

  do {
    // Lay out one page. Each page will become a fragment.
    const NGPhysicalBoxFragment* page =
        LayoutPage(page_index, page_name, break_token);

    if (page_name != page->PageName()) {
      // The page name changed. This may mean that the page size has changed as
      // well. We need to re-match styles and try again.
      //
      // Note: In many cases it could be possible to know the correct name of
      // the page before laying it out, by providing such information in the
      // break token, for instance. However, that's not going to work if the
      // very first page is named, since there's no break token then. So, given
      // that we may have to go back and re-layout in some cases, just do this
      // in all cases where named pages are involved, rather than having two
      // separate mechanisms. We could revisit this approach if it turns out to
      // be a performance problem (although that seems very unlikely).
      page_name = page->PageName();
      page = LayoutPage(page_index, page_name, break_token);
      DCHECK_EQ(page_name, page->PageName());
    }

    container_builder_.AddChild(*page, page_offset);

    LayoutUnit page_block_size =
        NGFragment(writing_direction, *page).BlockSize();
    intrinsic_block_size = std::max(intrinsic_block_size,
                                    page_offset.block_offset + page_block_size);
    page_offset.block_offset += page_block_size;
    break_token = page->BreakToken();
    page_index++;
  } while (break_token);

  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);

  // Compute the block-axis size now that we know our content size.
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), /* border_padding */ NGBoxStrut(),
      intrinsic_block_size, absl::nullopt);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGPageLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  NOTREACHED();
  return MinMaxSizesResult();
}

const NGPhysicalBoxFragment* NGPageLayoutAlgorithm::LayoutPage(
    uint32_t page_index,
    const AtomicString& page_name,
    const NGBlockBreakToken* break_token) const {
  const LayoutView* view = Node().GetDocument().GetLayoutView();
  WritingMode writing_mode = ConstraintSpace().GetWritingMode();
  LogicalSize page_size =
      view->PageAreaSize(page_index, page_name).ConvertToLogical(writing_mode);

  DCHECK(page_size.inline_size != kIndefiniteSize);
  DCHECK(page_size.block_size != kIndefiniteSize);
  NGConstraintSpace child_space = CreateConstraintSpaceForPages(page_size);
  NGFragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(child_space, Node(), BreakToken());
  NGBlockLayoutAlgorithm child_algorithm(
      {Node(), fragment_geometry, child_space, break_token});
  child_algorithm.SetBoxType(NGPhysicalFragment::kPageBox);
  const NGLayoutResult* result = child_algorithm.Layout();
  return &To<NGPhysicalBoxFragment>(result->PhysicalFragment());
}

NGConstraintSpace NGPageLayoutAlgorithm::CreateConstraintSpaceForPages(
    const LogicalSize& page_size) const {
  NGConstraintSpaceBuilder space_builder(
      ConstraintSpace(), Style().GetWritingDirection(), /* is_new_fc */ true);
  space_builder.SetAvailableSize(page_size);
  space_builder.SetPercentageResolutionSize(page_size);
  space_builder.SetInlineAutoBehavior(NGAutoBehavior::kStretchImplicit);

  space_builder.SetFragmentationType(kFragmentPage);
  space_builder.SetShouldPropagateChildBreakValues();
  space_builder.SetFragmentainerBlockSize(page_size.block_size);
  space_builder.SetIsAnonymous(true);

  return space_builder.ToConstraintSpace();
}

}  // namespace blink
