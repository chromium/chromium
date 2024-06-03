// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/page_border_box_layout_algorithm.h"

#include <algorithm>

#include "third_party/blink/renderer/core/layout/block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/simplified_oof_layout_algorithm.h"

namespace blink {

PageBorderBoxLayoutAlgorithm::PageBorderBoxLayoutAlgorithm(
    const LayoutAlgorithmParams& params,
    const BlockNode& content_node,
    const PageAreaLayoutParams& page_area_params)
    : LayoutAlgorithm(params),
      content_node_(content_node),
      page_area_params_(page_area_params) {}

const LayoutResult* PageBorderBoxLayoutAlgorithm::Layout() {
  DCHECK(!GetBreakToken());
  container_builder_.SetBoxType(PhysicalFragment::kPageBorderBox);

  // Lay out the contents of one page.
  ConstraintSpace fragmentainer_space = CreateConstraintSpaceForPageArea();
  FragmentGeometry fragment_geometry = CalculateInitialFragmentGeometry(
      fragmentainer_space, content_node_, /*break_token=*/nullptr);
  LayoutAlgorithmParams params(
      content_node_, fragment_geometry, fragmentainer_space,
      page_area_params_.break_token, /*early_break=*/nullptr);
  const LayoutResult* result;
  if (page_area_params_.template_fragmentainer) {
    // We are creating an empty fragmentainer for OutOfFlowLayoutPart to
    // populate with OOF children.
    SimplifiedOofLayoutAlgorithm algorithm(
        params, *page_area_params_.template_fragmentainer);
    result = algorithm.Layout();
  } else {
    BlockLayoutAlgorithm algorithm(params);
    algorithm.SetBoxType(PhysicalFragment::kPageArea);
    result = algorithm.Layout();
  }

  const auto& page = To<PhysicalBoxFragment>(result->GetPhysicalFragment());
  fragmentainer_break_token_ = page.GetBreakToken();

  // The page box is sized to fit the destination paper (if the destination is
  // an actual printer, and not PDF). Fragmented page content, on the other
  // hand, lives in a "stitched" coordinate system, potentially with a different
  // scale factor than the page border box, where all page areas have been
  // stitched together in the block direction, in order to allow overflowing
  // content on one page appear on another page (e.g. relative positioning or
  // tall monolithic content). Set the physical offset of the page area to 0,0,
  // so that we don't have to add work-arounds to ignore it on the paint side.
  WritingModeConverter converter(GetConstraintSpace().GetWritingDirection(),
                                 container_builder_.Size());
  LogicalOffset origin = converter.ToLogical(
      PhysicalOffset(), result->GetPhysicalFragment().Size());
  container_builder_.AddResult(*result, origin, /*margins=*/std::nullopt);

  return container_builder_.ToBoxFragment();
}

ConstraintSpace PageBorderBoxLayoutAlgorithm::CreateConstraintSpaceForPageArea()
    const {
  LogicalSize page_area_size = ChildAvailableSize();

  // Round up to the nearest integer. Although layout itself could have handled
  // subpixels just fine, the paint code cannot without bleeding across page
  // boundaries. The printing code (outside Blink) also rounds up. It's
  // important that all pieces of the machinery agree on which way to round, or
  // we risk clipping away a pixel or so at the edges. The reason for rounding
  // up (rather than down, or to the closest integer) is so that any box that
  // starts exactly at the beginning of a page, and uses a block-size exactly
  // equal to that of the page area (before rounding) will actually fit on one
  // page.
  page_area_size.inline_size = LayoutUnit(page_area_size.inline_size.Ceil());
  page_area_size.block_size = LayoutUnit(page_area_size.block_size.Ceil());

  // Use the writing mode of the document. The page context may have established
  // its own writing mode, but that shouldn't affect the writing mode of the
  // document contents.
  ConstraintSpaceBuilder space_builder(
      GetConstraintSpace(), content_node_.Style().GetWritingDirection(),
      /*is_new_fc=*/true);

  space_builder.SetAvailableSize(page_area_size);
  space_builder.SetPercentageResolutionSize(page_area_size);
  space_builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchImplicit);
  space_builder.SetBlockAutoBehavior(AutoSizeBehavior::kStretchImplicit);

  space_builder.SetFragmentationType(kFragmentPage);
  space_builder.SetShouldPropagateChildBreakValues();
  space_builder.SetFragmentainerBlockSizeFromAvailableSize();
  space_builder.SetIsAnonymous(true);

  return space_builder.ToConstraintSpace();
}

}  // namespace blink
