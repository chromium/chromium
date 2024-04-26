// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/page_border_box_layout_algorithm.h"

#include <algorithm>

#include "third_party/blink/renderer/core/layout/block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
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
  container_builder_.AddResult(*result, LogicalOffset(),
                               /*margins=*/std::nullopt);

  return container_builder_.ToBoxFragment();
}

ConstraintSpace PageBorderBoxLayoutAlgorithm::CreateConstraintSpaceForPageArea()
    const {
  LogicalSize page_area_size = GetConstraintSpace().AvailableSize();

  ConstraintSpaceBuilder space_builder(
      GetConstraintSpace(), Style().GetWritingDirection(), /*is_new_fc=*/true);
  space_builder.SetAvailableSize(page_area_size);
  space_builder.SetPercentageResolutionSize(page_area_size);
  space_builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchImplicit);
  space_builder.SetBlockAutoBehavior(AutoSizeBehavior::kStretchImplicit);

  space_builder.SetFragmentationType(kFragmentPage);
  space_builder.SetShouldPropagateChildBreakValues();
  space_builder.SetFragmentainerBlockSize(page_area_size.block_size);
  space_builder.SetIsAnonymous(true);

  return space_builder.ToConstraintSpace();
}

}  // namespace blink
