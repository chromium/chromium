// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/page_container_layout_algorithm.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/pagination_state.h"
#include "third_party/blink/renderer/core/layout/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/page_border_box_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/pagination_utils.h"

namespace blink {

PageContainerLayoutAlgorithm::PageContainerLayoutAlgorithm(
    const LayoutAlgorithmParams& params,
    wtf_size_t page_index,
    const AtomicString& page_name,
    const BlockNode& content_node,
    const PageAreaLayoutParams& page_area_params,
    bool ignore_author_page_style)
    : LayoutAlgorithm(params),
      page_index_(page_index),
      page_name_(page_name),
      content_node_(content_node),
      page_area_params_(page_area_params),
      ignore_author_page_style_(ignore_author_page_style) {}

const LayoutResult* PageContainerLayoutAlgorithm::Layout() {
  DCHECK(!GetBreakToken());
  container_builder_.SetBoxType(PhysicalFragment::kPageContainer);

  Document& document = Node().GetDocument();
  float layout_scale = document.GetLayoutView()->PaginationScaleFactor();
  LogicalSize containing_block_size =
      DesiredPageContainingBlockSize(document, Style()) * layout_scale;

  const ComputedStyle* content_scaled_style = &Style();
  if (layout_scale != 1 && !ignore_author_page_style_) {
    // Scaling shouldn't apply to @page borders etc. Apply a zoom property to
    // cancel out the effect of layout scaling.
    content_scaled_style = document.GetStyleResolver().StyleForPage(
        page_index_, page_name_, layout_scale);
  }

  LayoutBlockFlow* page_border_box =
      document.View()->GetPaginationState()->CreateAnonymousPageLayoutObject(
          document, *content_scaled_style);
  BlockNode page_border_box_node(page_border_box);

  FragmentGeometry geometry;
  ResolvePageBoxGeometry(page_border_box_node, containing_block_size,
                         &geometry);

  LayoutAlgorithmParams params(page_border_box_node, geometry,
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
