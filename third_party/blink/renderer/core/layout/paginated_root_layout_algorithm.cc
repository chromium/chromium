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
#include "third_party/blink/renderer/core/layout/out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/page_border_box_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/page_container_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/pagination_utils.h"
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
  const ComputedStyle* page_container_style =
      document.GetStyleResolver().StyleForPage(page_index, page_name);

  LayoutBlockFlow* page_container =
      document.View()->GetPaginationState()->CreateAnonymousPageLayoutObject(
          document, *page_container_style);
  BlockNode page_container_node(page_container);

  // Calculate the page border box size based on @page properties, such as
  // 'size' and 'margin', but also padding, width, height, min-height, and so
  // on. Auto margins will be resolved. One interesting detail here is how
  // over-constrainedness is handled. Although, for regular CSS boxes, margins
  // will be adjusted to resolve it, for page boxes, the containing block size
  // (the one set by the 'size' descriptor / property) is adjusted instead.
  //
  // Example: @page { size:500px; margin:50px; width:100px; }
  //
  // The equation (omitting border and padding, since they are 0 in this
  // example):
  // 'margin-left' + 'width' + 'margin-right' = width of containing block
  //
  // The width of the containing block is 500px (from size). This is what needs
  // to be adjusted to resolve the overconstraintedness - i.e. it needs to
  // become 50+100+50=200. So we end up with a page box size of 200x500, and a
  // page area size of 100x400.
  //
  // https://drafts.csswg.org/css-page-3/#page-model
  FragmentGeometry geometry;
  BoxStrut margins;
  LogicalSize page_containing_block_size =
      DesiredPageContainingBlockSize(document, *page_container_style);
  ResolvePageBoxGeometry(page_container_node, page_containing_block_size,
                         &geometry, &margins);

  // Check if the resulting page area size is usable.
  LogicalSize desired_page_area_size =
      geometry.border_box_size - geometry.border - geometry.padding;
  bool ignore_author_page_style = false;
  if (desired_page_area_size.inline_size < LayoutUnit(1) ||
      desired_page_area_size.block_size < LayoutUnit(1)) {
    // The resulting page area size would become zero (or very close to
    // it). Ignore CSS, and use the default values provided as input. There are
    // tests that currently expect this behavior. But see
    // https://github.com/w3c/csswg-drafts/issues/8335
    ignore_author_page_style = true;
    page_container_style = document.GetStyleResolver().StyleForPage(
        page_index, page_name, 1.0, ignore_author_page_style);
    page_container->SetStyle(page_container_style,
                             LayoutObject::ApplyStyleChanges::kNo);
    page_containing_block_size =
        DesiredPageContainingBlockSize(document, *page_container_style);
    ResolvePageBoxGeometry(page_container_node, page_containing_block_size,
                           &geometry, &margins);
  }

  // Convert from border box size to margin box size, and use that to calculate
  // the final page container size. If the destination is a printer, i.e. so
  // that there's a given paper size, the resulting size will be that of the
  // paper, honoring the orientation implied by the margin box size. If the
  // destination is PDF, on the other hand, no fitting will be required.
  LogicalSize margin_box_size(geometry.border_box_size + margins);
  LogicalSize page_container_size = FittedPageContainerSize(
      document, page_container_node.Style(), margin_box_size);

  ConstraintSpaceBuilder space_builder(
      parent_space, page_container_style->GetWritingDirection(),
      /*is_new_fc=*/true);
  SetUpSpaceBuilderForPageBox(page_container_size, &space_builder);
  space_builder.SetShouldPropagateChildBreakValues();
  ConstraintSpace child_space = space_builder.ToConstraintSpace();

  FragmentGeometry margin_box_geometry = {.border_box_size =
                                              page_container_size};

  LayoutAlgorithmParams params(page_container_node, margin_box_geometry,
                               child_space, /*break_token=*/nullptr);
  PageContainerLayoutAlgorithm child_algorithm(params, page_index, page_name,
                                               root_node, page_area_params,
                                               ignore_author_page_style);
  const LayoutResult* result = child_algorithm.Layout();

  // Since we didn't lay out via BlockNode::Layout(), but rather picked and
  // initialized a child layout algorithm on our own, we have some additional
  // work to invoke on our own:
  page_container_node.FinishPageContainerLayout(result);

  return PageContainerResult(
      To<PhysicalBoxFragment>(result->GetPhysicalFragment()),
      child_algorithm.FragmentainerBreakToken());
}

}  // namespace blink
