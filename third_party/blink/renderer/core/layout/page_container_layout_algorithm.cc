// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/page_container_layout_algorithm.h"

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/pagination_state.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/page_border_box_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/pagination_utils.h"

namespace blink {

namespace {

LogicalRect SnappedBorderBoxRect(const LogicalRect& rect) {
  // Some considerations here: The offset should be integers, since a
  // translation transform will be applied when printing, and everything will
  // look blurry otherwise. The value should be rounded to the nearest integer
  // (not ceil / floor), to match what it would look like if the same offset
  // were applied from within the document contents (e.g. margin / padding on a
  // regular DIV). The size needs to be rounded up to the nearest integer, to
  // match the page area size used during layout. This is rounded up mainly so
  // that authors may assume that an element with the same block-size as the
  // specified page size will fit on one page.
  return LogicalRect(LayoutUnit(rect.offset.inline_offset.Round()),
                     LayoutUnit(rect.offset.block_offset.Round()),
                     LayoutUnit(rect.size.inline_size.Ceil()),
                     LayoutUnit(rect.size.block_size.Ceil()));
}

}  // anonymous namespace

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

  // The size of a page container will always match the size of the destination
  // (if the destination is actual paper, this is given by the paper size - if
  // it's PDF, the destination size will be calculated solely using the input
  // page size and @page properties).
  //
  // The page border box, on the other hand, is in the coordinate system of
  // layout, which means that it will be affected by @page properties, even if
  // there's a given paper size. When painting the paginated layout, the page
  // border box will be scaled down to fit the paper if necessary, and then
  // centered.
  //
  // If the page size computed from @page properties is smaller than the actual
  // paper, there will be a gap between the page margins and the page border
  // box. This gap will not be used for anything, i.e. the specified margins
  // will left as-is, not expanded.
  //
  // Example: The paper size is 816x1056 (US Letter). Margins are 50px on each
  // side. The size of the page container becomes 816x1056. @page size is 500px
  // (a square). The page border box size will become 400x400 (margins
  // subtracted). The page border box will be centered on paper, meaning that
  // the page border box offset will be 208,328 ((816-400)/2, (1056-400)/2).
  // This does *not* mean that the left,top margins will be 208,328; they remain
  // at 50px. This what will be available to browser-generated headers / footers
  // (and @page margin boxes).
  //
  // When the page size computed from @page properties is larger than the actual
  // paper, it needs to be scaled down before it can be centered. Since it's the
  // page container (and not the page area) that is being scaled down (this is
  // how it's "always" been in Chromium, and it does make sense in a way, since
  // it reduces the amount of shrinking needed), the margins may also need to be
  // shrunk. Example: The paper size is 816x1056 (US Letter). Margins are
  // specified as 50px on each side (from print settings or from @page - doesn't
  // matter). The size of the page container becomes 816x1056. @page size is
  // 1632px (a square). The scale factor will be min(816/1632, 1056/1632) =
  // 0.5. The page border box size used by layout will be 1532x1532 (margins
  // subtracted). When painted, it will be scaled down to 766x766. When centered
  // on paper, we're going to need a border box left,top offset of 25,145. The
  // remaining width after scaling down the page border box is 816-766=50. The
  // remaining height is 1056-766=290. There's 50px of available width for
  // horizontal margins, but they actually wanted 50px each. So they need to be
  // adjusted to 25px. There's 290px of available height for vertical
  // margins. They wanted 50px each, and will be kept at that value.
  //
  // We now need to figure out how large the page "wants to be" in layout
  // (source), compare to constraints given by the physical paper size, scale it
  // down, adjust margins, and center on the sheet accordingly (destination).
  FragmentGeometry unscaled_geometry;
  BoxStrut margins;
  LogicalSize containing_block_size =
      DesiredPageContainingBlockSize(document, Style());
  ResolvePageBoxGeometry(Node(), containing_block_size, &unscaled_geometry,
                         &margins);

  LogicalSize source_page_margin_box_size(
      unscaled_geometry.border_box_size.inline_size + margins.InlineSum(),
      unscaled_geometry.border_box_size.block_size + margins.BlockSum());
  LogicalRect target_page_border_box_rect = TargetPageBorderBoxLogicalRect(
      document, Style(), source_page_margin_box_size, margins);
  target_page_border_box_rect =
      SnappedBorderBoxRect(target_page_border_box_rect);

  // The offset of the page border box is in the coordinate system of the target
  // (fitting to the sheet of paper, if applicable, for instance), whereas the
  // *size* of the page border box is in the coordinate system of layout (which
  // honors @page size, and various sorts of scaling). We now need to help the
  // fragment builder a bit, so that it ends up with the correct physical target
  // offset in the end.
  WritingModeConverter converter(Style().GetWritingDirection(),
                                 GetConstraintSpace().AvailableSize());
  PhysicalRect border_box_physical_rect =
      converter.ToPhysical(target_page_border_box_rect);
  // We have the correct physical offset in the target coordinate system here,
  // but in order to calculate the corresponding logical offset, we need to
  // convert it against the margin box size in the layout coordinate system, so
  // that, when the fragment builder eventually wants to calculate the physical
  // offset, it will get it right, by converting against the fragment's border
  // box size (which is in the layout coordinate system), with the outer size
  // being the target ("paper") size.
  border_box_physical_rect.size =
      converter.ToPhysical(unscaled_geometry.border_box_size);
  LogicalOffset target_offset =
      converter.ToLogical(border_box_physical_rect).offset;

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
  ResolvePageBoxGeometry(page_border_box_node,
                         containing_block_size * layout_scale, &geometry);

  LayoutAlgorithmParams params(page_border_box_node, geometry,
                               GetConstraintSpace(), /*break_token=*/nullptr);
  PageBorderBoxLayoutAlgorithm child_algorithm(params, content_node_,
                                               page_area_params_);
  const LayoutResult* result = child_algorithm.Layout();

  // Since we didn't lay out via BlockNode::Layout(), but rather picked and
  // initialized a child layout algorithm on our own, we have some additional
  // work to invoke on our own:
  page_border_box_node.FinishPageContainerLayout(result);

  container_builder_.AddResult(*result, target_offset,
                               /*margins=*/std::nullopt);

  fragmentainer_break_token_ = child_algorithm.FragmentainerBreakToken();

  // TODO(mstensho): Lay out page margin boxes here.

  return container_builder_.ToBoxFragment();
}

}  // namespace blink
