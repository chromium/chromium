// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/page_container_layout_algorithm.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/pagination_state.h"
#include "third_party/blink/renderer/core/layout/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/page_border_box_layout_algorithm.h"

namespace blink {

PageContainerLayoutAlgorithm::PageContainerLayoutAlgorithm(
    const LayoutAlgorithmParams& params,
    const BlockNode& content_node,
    const PageAreaLayoutParams& page_area_params)
    : LayoutAlgorithm(params),
      content_node_(content_node),
      page_area_params_(page_area_params) {}

const LayoutResult* PageContainerLayoutAlgorithm::Layout() {
  DCHECK(!GetBreakToken());
  container_builder_.SetBoxType(PhysicalFragment::kPageContainer);

  Document& document = Node().GetDocument();

  // TODO(mstensho): The page container should include margins (which it
  // currently doesn't), whereas they should not be part of the page border box.
  FragmentGeometry fragment_geometry = {
      .border_box_size = GetConstraintSpace().AvailableSize()};

  LayoutBlockFlow* page_border_box =
      document.View()->GetPaginationState()->CreateAnonymousPageLayoutObject(
          document, Style());
  BlockNode page_border_box_node(page_border_box);

  LayoutAlgorithmParams params(page_border_box_node, fragment_geometry,
                               GetConstraintSpace(), /*break_token=*/nullptr);
  PageBorderBoxLayoutAlgorithm child_algorithm(params, content_node_,
                                               page_area_params_);
  const LayoutResult* result = child_algorithm.Layout();

  // Since we didn't lay out via BlockNode::Layout(), but rather picked and
  // initialized a child layout algorithm on our own, we have some additional
  // work to invoke on our own:
  page_border_box_node.FinishPageContainerLayout(result);

  // TODO(mstensho): Offset by page margins.
  container_builder_.AddResult(*result, LogicalOffset(),
                               /*margins=*/std::nullopt);

  fragmentainer_break_token_ = child_algorithm.FragmentainerBreakToken();

  // TODO(mstensho): Lay out page margin boxes here.

  return container_builder_.ToBoxFragment();
}

}  // namespace blink
