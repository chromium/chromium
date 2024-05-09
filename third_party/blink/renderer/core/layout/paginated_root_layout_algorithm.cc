// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/paginated_root_layout_algorithm.h"

#include <algorithm>

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/pagination_state.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/page_border_box_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/page_container_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

PaginatedRootLayoutAlgorithm::PaginatedRootLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params) {}

const LayoutResult* PaginatedRootLayoutAlgorithm::Layout() {
  DCHECK(!GetBreakToken());
  WritingModeConverter converter(GetConstraintSpace().GetWritingDirection(),
                                 container_builder_.Size());
  wtf_size_t page_index = 0;
  AtomicString page_name;

  container_builder_.SetIsBlockFragmentationContextRoot();

  PageAreaLayoutParams page_area_params;
  do {
    PageContainerResult result =
        LayoutPageContainer(page_index, page_name, page_area_params);
    // Lay out one page. Each page will become a fragment.

    if (page_name != result.fragment->PageName()) {
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
      page_name = result.fragment->PageName();
      result = LayoutPageContainer(page_index, page_name, page_area_params);
      DCHECK_EQ(page_name, result.fragment->PageName());
    }

    // Each page container establishes its own coordinate system, without any
    // relationship to other page containers (there *is* a relationship on the
    // document contents side of things (stitched coordinate system), but that's
    // not relevant here). Set the physical offset of the page container to 0,0,
    // so that we don't have to add work-arounds to ignore it on the paint side.
    LogicalOffset origin =
        converter.ToLogical(PhysicalOffset(), result.fragment->Size());
    container_builder_.AddChild(*result.fragment, origin);

    page_area_params.break_token = result.fragmentainer_break_token;
    page_index++;
  } while (page_area_params.break_token);

  // Compute the block-axis size now that we know our content size.
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      GetConstraintSpace(), Style(), /*border_padding=*/BoxStrut(),
      /*intrinsic_size=*/LayoutUnit(), kIndefiniteSize);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  OutOfFlowLayoutPart(Node(), GetConstraintSpace(), &container_builder_).Run();

  return container_builder_.ToBoxFragment();
}

const PhysicalBoxFragment& PaginatedRootLayoutAlgorithm::CreateEmptyPage(
    const BlockNode& node,
    const ConstraintSpace& parent_space,
    wtf_size_t page_index,
    const PhysicalBoxFragment& previous_fragmentainer) {
  const BlockBreakToken* break_token = previous_fragmentainer.GetBreakToken();
  PageAreaLayoutParams page_area_params = {
      .break_token = break_token,
      .template_fragmentainer = &previous_fragmentainer};
  PageContainerResult result =
      LayoutPageContainer(node, parent_space, page_index,
                          previous_fragmentainer.PageName(), page_area_params);
  return *result.fragment;
}

PaginatedRootLayoutAlgorithm::PageContainerResult
PaginatedRootLayoutAlgorithm::LayoutPageContainer(
    const BlockNode& root_node,
    const ConstraintSpace& parent_space,
    wtf_size_t page_index,
    const AtomicString& page_name,
    const PageAreaLayoutParams& page_area_params) {
  Document& document = root_node.GetDocument();
  const LayoutView* view = document.GetLayoutView();
  WritingMode writing_mode = parent_space.GetWritingMode();
  LogicalSize page_size =
      view->PageAreaSize(page_index, page_name).ConvertToLogical(writing_mode);

  DCHECK(page_size.inline_size != kIndefiniteSize);
  DCHECK(page_size.block_size != kIndefiniteSize);
  const ComputedStyle* page_container_style =
      document.GetStyleResolver().StyleForPage(page_index, page_name);

  LayoutBlockFlow* page_container =
      document.View()->GetPaginationState()->CreateAnonymousPageLayoutObject(
          document, *page_container_style);
  BlockNode page_container_node(page_container);

  ConstraintSpace child_space =
      CreateConstraintSpaceForPages(root_node, parent_space, page_size);
  FragmentGeometry fragment_geometry = CalculateInitialFragmentGeometry(
      child_space, root_node, /*break_token=*/nullptr);
  LayoutAlgorithmParams params(page_container_node, fragment_geometry,
                               child_space, /*break_token=*/nullptr);
  PageContainerLayoutAlgorithm child_algorithm(params, root_node,
                                               page_area_params);
  const LayoutResult* result = child_algorithm.Layout();

  // Since we didn't lay out via BlockNode::Layout(), but rather picked and
  // initialized a child layout algorithm on our own, we have some additional
  // work to invoke on our own:
  page_container_node.FinishPageContainerLayout(result);

  return PageContainerResult(
      To<PhysicalBoxFragment>(result->GetPhysicalFragment()),
      child_algorithm.FragmentainerBreakToken());
}

ConstraintSpace PaginatedRootLayoutAlgorithm::CreateConstraintSpaceForPages(
    const BlockNode& node,
    const ConstraintSpace& space,
    const LogicalSize& page_size) {
  ConstraintSpaceBuilder space_builder(
      space, node.Style().GetWritingDirection(), /*is_new_fc=*/true);
  space_builder.SetAvailableSize(page_size);
  space_builder.SetPercentageResolutionSize(page_size);
  space_builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchImplicit);
  space_builder.SetBlockAutoBehavior(AutoSizeBehavior::kStretchImplicit);
  space_builder.SetShouldPropagateChildBreakValues();

  return space_builder.ToConstraintSpace();
}

}  // namespace blink
