// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/layout/page_container_layout_algorithm.h"

#include "third_party/blink/renderer/core/css/page_margins_style.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/pagination_state.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_quote.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/logical_fragment.h"
#include "third_party/blink/renderer/core/layout/page_border_box_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/pagination_utils.h"
#include "third_party/blink/renderer/core/style/content_data.h"

namespace blink {

namespace {

void PrepareMarginBoxSpaceBuilder(LogicalSize available_size,
                                  ConstraintSpaceBuilder* builder) {
  builder->SetAvailableSize(available_size);
  builder->SetInlineAutoBehavior(AutoSizeBehavior::kStretchImplicit);
  builder->SetBlockAutoBehavior(AutoSizeBehavior::kStretchImplicit);
  builder->SetDecorationPercentageResolutionType(
      DecorationPercentageResolutionType::kContainingBlockSize);

  // Each page-margin box always establishes a stacking context.
  builder->SetIsPaintedAtomically(true);
}

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
    wtf_size_t total_page_count,
    const AtomicString& page_name,
    const BlockNode& content_node,
    const CountersAttachmentContext& counters_context,
    const PageAreaLayoutParams& page_area_params,
    bool ignore_author_page_style,
    const PhysicalBoxFragment* existing_page_container)
    : LayoutAlgorithm(params),
      page_index_(page_index),
      total_page_count_(total_page_count),
      page_name_(page_name),
      content_node_(content_node),
      counters_context_(counters_context.DeepClone()),
      page_area_params_(page_area_params),
      ignore_author_page_style_(ignore_author_page_style),
      existing_page_container_(existing_page_container) {}

const LayoutResult* PageContainerLayoutAlgorithm::Layout() {
  DCHECK(!GetBreakToken());
  container_builder_.SetBoxType(PhysicalFragment::kPageContainer);

  Document& document = Node().GetDocument();

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

  counters_context_.EnterObject(*Node().GetLayoutBox(), /*is_page_box=*/true);

  LayoutPageBorderBox(containing_block_size, target_offset);

  // Paper fitting may require margins to be reduced. If contents are scaled
  // down to fit, so are the margins.
  BoxStrut minimal_margins(GetConstraintSpace().AvailableSize(),
                           target_page_border_box_rect);
  margins.Intersect(minimal_margins);

  LayoutAllMarginBoxes(margins);

  counters_context_.LeaveObject(*Node().GetLayoutBox(), /*is_page_box=*/true);

  return container_builder_.ToBoxFragment();
}

void PageContainerLayoutAlgorithm::LayoutPageBorderBox(
    LogicalSize containing_block_size,
    LogicalOffset target_offset) {
  if (existing_page_container_) {
    // A page container was created previously. But we had to come back and
    // update the total page count (counter(pages)). We can just keep the old
    // page border box (including all paginated content), though. No need for a
    // re-layout.
    const PhysicalBoxFragment& page_border_box_fragment =
        GetPageBorderBox(*existing_page_container_);
    const LayoutResult* existing_border_box_result =
        page_border_box_fragment.OwnerLayoutBox()->GetLayoutResult(0);
    container_builder_.AddResult(*existing_border_box_result, target_offset,
                                 /*margins=*/std::nullopt);
    return;
  }

  Document& document = Node().GetDocument();
  const LayoutView& layout_view = *document.GetLayoutView();
  const ComputedStyle* content_scaled_style = &Style();
  float layout_scale = layout_view.PaginationScaleFactor();
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

  ConstraintSpaceBuilder space_builder(GetConstraintSpace(),
                                       Style().GetWritingDirection(),
                                       /*is_new_fc=*/true);
  space_builder.SetAvailableSize(GetConstraintSpace().AvailableSize());
  space_builder.SetIsPaintedAtomically(true);
  ConstraintSpace child_space = space_builder.ToConstraintSpace();
  LayoutAlgorithmParams params(page_border_box_node, geometry, child_space,
                               /*break_token=*/nullptr);
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
}

void PageContainerLayoutAlgorithm::LayoutAllMarginBoxes(
    const BoxStrut& logical_margins) {
  Document& document = Node().GetDocument();
  PageMarginsStyle margins_style;
  document.GetStyleResolver().StyleForPageMargins(Style(), page_index_,
                                                  page_name_, &margins_style);

  // Margin boxes are positioned according to their type physically - meaning
  // that e.g. @top-left-corner always means the top left corner, regardless of
  // writing mode. Although layout works on logical sizes and offsets, it's less
  // confusing here to use physical ones (and convert to logical values right
  // before entering layout), rather than inventing a bunch of writing-mode
  // agnositic terminology that doesn't exist in the spec.
  PhysicalSize page_box_size =
      ToPhysicalSize(GetConstraintSpace().AvailableSize(),
                     GetConstraintSpace().GetWritingMode());
  PhysicalBoxStrut margins = logical_margins.ConvertToPhysical(
      GetConstraintSpace().GetWritingDirection());
  LayoutUnit right_edge = page_box_size.width - margins.right;
  LayoutUnit bottom_edge = page_box_size.height - margins.bottom;

  PhysicalRect top_left_corner_rect(LayoutUnit(), LayoutUnit(), margins.left,
                                    margins.top);
  PhysicalRect top_right_corner_rect(right_edge, LayoutUnit(), margins.right,
                                     margins.top);
  PhysicalRect bottom_right_corner_rect(right_edge, bottom_edge, margins.right,
                                        margins.bottom);
  PhysicalRect bottom_left_corner_rect(LayoutUnit(), bottom_edge, margins.left,
                                       margins.bottom);
  PhysicalRect top_edge_rect(margins.left, LayoutUnit(),
                             page_box_size.width - margins.HorizontalSum(),
                             margins.top);
  PhysicalRect right_edge_rect(right_edge, margins.top, margins.right,
                               page_box_size.height - margins.VerticalSum());
  PhysicalRect bottom_edge_rect(margins.left, bottom_edge,
                                page_box_size.width - margins.HorizontalSum(),
                                margins.bottom);
  PhysicalRect left_edge_rect(LayoutUnit(), margins.top, margins.left,
                              page_box_size.height - margins.VerticalSum());

  // Lay out in default paint order. Start in the top left corner and go
  // clockwise. See https://drafts.csswg.org/css-page-3/#painting
  LayoutCornerMarginNode(margins_style[PageMarginsStyle::TopLeftCorner],
                         top_left_corner_rect, TopEdge | LeftEdge);
  LayoutEdgeMarginNodes(margins_style[PageMarginsStyle::TopLeft],
                        margins_style[PageMarginsStyle::TopCenter],
                        margins_style[PageMarginsStyle::TopRight],
                        top_edge_rect, TopEdge);
  LayoutCornerMarginNode(margins_style[PageMarginsStyle::TopRightCorner],
                         top_right_corner_rect, TopEdge | RightEdge);
  LayoutEdgeMarginNodes(margins_style[PageMarginsStyle::RightTop],
                        margins_style[PageMarginsStyle::RightMiddle],
                        margins_style[PageMarginsStyle::RightBottom],
                        right_edge_rect, RightEdge);
  LayoutCornerMarginNode(margins_style[PageMarginsStyle::BottomRightCorner],
                         bottom_right_corner_rect, BottomEdge | RightEdge);
  LayoutEdgeMarginNodes(margins_style[PageMarginsStyle::BottomLeft],
                        margins_style[PageMarginsStyle::BottomCenter],
                        margins_style[PageMarginsStyle::BottomRight],
                        bottom_edge_rect, BottomEdge);
  LayoutCornerMarginNode(margins_style[PageMarginsStyle::BottomLeftCorner],
                         bottom_left_corner_rect, BottomEdge | LeftEdge);
  LayoutEdgeMarginNodes(margins_style[PageMarginsStyle::LeftTop],
                        margins_style[PageMarginsStyle::LeftMiddle],
                        margins_style[PageMarginsStyle::LeftBottom],
                        left_edge_rect, LeftEdge);

  container_builder_.SetFragmentsTotalBlockSize(
      container_builder_.FragmentBlockSize());
}

void PageContainerLayoutAlgorithm::LayoutCornerMarginNode(
    const ComputedStyle* corner_style,
    const PhysicalRect& rect,
    EdgeAdjacency edge_adjacency) {
  BlockNode corner_node = CreateBlockNodeIfNeeded(corner_style);
  if (!corner_node) {
    return;
  }

  ConstraintSpaceBuilder space_builder(GetConstraintSpace(),
                                       corner_style->GetWritingDirection(),
                                       /*is_new_fc=*/true);
  WritingModeConverter converter(Style().GetWritingDirection(),
                                 GetConstraintSpace().AvailableSize());
  LogicalRect logical_rect = converter.ToLogical(rect);
  PrepareMarginBoxSpaceBuilder(logical_rect.size, &space_builder);
  ConstraintSpace child_space = space_builder.ToConstraintSpace();

  const LayoutResult* result = corner_node.Layout(child_space);
  const auto& box_fragment =
      To<PhysicalBoxFragment>(result->GetPhysicalFragment());
  PhysicalBoxStrut physical_margins =
      ResolveMargins(child_space, *corner_style, box_fragment.Size(), rect.size,
                     edge_adjacency);
  BoxStrut logical_margins =
      physical_margins.ConvertToLogical(Style().GetWritingDirection());
  LogicalOffset offset = logical_rect.offset + logical_margins.StartOffset();
  container_builder_.AddResult(*result, offset);
}

void PageContainerLayoutAlgorithm::LayoutEdgeMarginNodes(
    const ComputedStyle* start_box_style,
    const ComputedStyle* center_box_style,
    const ComputedStyle* end_box_style,
    const PhysicalRect& edge_rect,
    EdgeAdjacency edge_adjacency) {
  BlockNode nodes[3] = {CreateBlockNodeIfNeeded(start_box_style),
                        CreateBlockNodeIfNeeded(center_box_style),
                        CreateBlockNodeIfNeeded(end_box_style)};
  LayoutUnit main_axis_sizes[3];

  ProgressionDirection dir;
  switch (edge_adjacency) {
    case TopEdge:
      dir = LeftToRight;
      break;
    case RightEdge:
      dir = TopToBottom;
      break;
    case BottomEdge:
      dir = RightToLeft;
      break;
    case LeftEdge:
      dir = BottomToTop;
      break;
    default:
      NOTREACHED();
  }

  CalculateEdgeMarginBoxSizes(edge_rect.size, nodes, dir, main_axis_sizes);

  if (IsReverse(dir)) {
    LayoutEdgeMarginNode(nodes[EndMarginBox], edge_rect,
                         main_axis_sizes[EndMarginBox], EndMarginBox,
                         edge_adjacency, dir);
    LayoutEdgeMarginNode(nodes[CenterMarginBox], edge_rect,
                         main_axis_sizes[CenterMarginBox], CenterMarginBox,
                         edge_adjacency, dir);
    LayoutEdgeMarginNode(nodes[StartMarginBox], edge_rect,
                         main_axis_sizes[StartMarginBox], StartMarginBox,
                         edge_adjacency, dir);
  } else {
    LayoutEdgeMarginNode(nodes[StartMarginBox], edge_rect,
                         main_axis_sizes[StartMarginBox], StartMarginBox,
                         edge_adjacency, dir);
    LayoutEdgeMarginNode(nodes[CenterMarginBox], edge_rect,
                         main_axis_sizes[CenterMarginBox], CenterMarginBox,
                         edge_adjacency, dir);
    LayoutEdgeMarginNode(nodes[EndMarginBox], edge_rect,
                         main_axis_sizes[EndMarginBox], EndMarginBox,
                         edge_adjacency, dir);
  }
}

BlockNode PageContainerLayoutAlgorithm::CreateBlockNodeIfNeeded(
    const ComputedStyle* page_margin_style) {
  if (!page_margin_style) {
    return BlockNode(nullptr);
  }
  const ContentData* content = page_margin_style->GetContentData();
  if (!content) {
    return BlockNode(nullptr);
  }

  Document& document = Node().GetDocument();
  LayoutBlockFlow* margin_layout_box =
      document.View()->GetPaginationState()->CreateAnonymousPageLayoutObject(
          document, *page_margin_style);

  counters_context_.EnterObject(*margin_layout_box);

  int quote_depth = 0;
  for (; content; content = content->Next()) {
    if (content->IsAltText() || content->IsNone()) {
      continue;
    }
    LayoutObject* child = content->CreateLayoutObject(*margin_layout_box);
    if (margin_layout_box->IsChildAllowed(child, *page_margin_style)) {
      margin_layout_box->AddChild(child);

      if (auto* quote = DynamicTo<LayoutQuote>(child)) {
        quote->SetDepth(quote_depth);
        quote->UpdateText();
        quote_depth = quote->GetNextDepth();
      } else if (auto* counter = DynamicTo<LayoutCounter>(child)) {
        Vector<int> values;
        const auto* counter_data = To<CounterContentData>(content);
        if (counter_data->Identifier() == "pages") {
          if (!total_page_count_) {
            // Someone wants to output the total page count. In order to
            // calculate a total page count, we first have to lay out all pages,
            // and then come back for a second pass.
            DCHECK(!existing_page_container_);
            needs_total_page_count_ = true;
          }
          values.push_back(total_page_count_);
        } else {
          values = counters_context_.GetCounterValues(
              *Node().GetLayoutBox(), counter->Identifier(),
              counter->Separator().IsNull());
        }
        counter->UpdateCounter(std::move(values));
      }
    } else {
      child->Destroy();
    }
  }

  counters_context_.LeaveObject(*margin_layout_box);

  if (!margin_layout_box->FirstChild()) {
    // No content was added.
    margin_layout_box = nullptr;
  }

  return BlockNode(margin_layout_box);
}

PageContainerLayoutAlgorithm::PreferredSizeInfo
PageContainerLayoutAlgorithm::EdgeMarginNodePreferredSize(
    const BlockNode& child,
    LogicalSize containing_block_size,
    ProgressionDirection dir) const {
  DCHECK(child);
  ConstraintSpaceBuilder space_builder(GetConstraintSpace(),
                                       child.Style().GetWritingDirection(),
                                       /*is_new_fc=*/true);

  space_builder.SetAvailableSize(containing_block_size);
  space_builder.SetDecorationPercentageResolutionType(
      DecorationPercentageResolutionType::kContainingBlockSize);
  ConstraintSpace child_space = space_builder.ToConstraintSpace();
  BoxStrut margins = ComputeMarginsForSelf(child_space, child.Style());

  MinMaxSizes minmax;
  LayoutUnit margin_sum;

  bool main_axis_is_inline_for_child =
      IsHorizontal(dir) == child.Style().IsHorizontalWritingMode();
  bool main_axis_is_auto;
  if (main_axis_is_inline_for_child) {
    main_axis_is_auto = child.Style().LogicalWidth().IsAuto();
    if (main_axis_is_auto) {
      ConstraintSpaceBuilder intrinsic_space_builder(
          GetConstraintSpace(), child.Style().GetWritingDirection(),
          /*is_new_fc=*/true);
      intrinsic_space_builder.SetCacheSlot(LayoutResultCacheSlot::kMeasure);
      minmax = ComputeMinAndMaxContentContributionForSelf(
                   child, intrinsic_space_builder.ToConstraintSpace())
                   .sizes;
    } else {
      BoxStrut border_padding = ComputeBorders(child_space, child) +
                                ComputePadding(child_space, child.Style());
      minmax = ComputeInlineSizeForFragment(child_space, child, border_padding);
    }
    margin_sum = margins.InlineSum();
  } else {
    // Need to lay out for block-sizes.
    main_axis_is_auto = child.Style().LogicalHeight().IsAuto();
    const LayoutResult* result = child.Layout(child_space);
    LogicalSize size = result->GetPhysicalFragment().Size().ConvertToLogical(
        child_space.GetWritingMode());
    minmax.min_size = size.block_size;
    minmax.max_size = size.block_size;
    margin_sum = margins.BlockSum();
  }

  return PreferredSizeInfo(minmax, margin_sum, main_axis_is_auto);
}

void PageContainerLayoutAlgorithm::CalculateEdgeMarginBoxSizes(
    PhysicalSize available_physical_size,
    const BlockNode nodes[3],
    ProgressionDirection dir,
    LayoutUnit final_main_axis_sizes[3]) const {
  LayoutUnit available_main_axis_size;
  if (IsHorizontal(dir)) {
    available_main_axis_size = available_physical_size.width;
  } else {
    available_main_axis_size = available_physical_size.height;
  }

  LogicalSize available_logical_size =
      available_physical_size.ConvertToLogical(Style().GetWritingMode());
  PreferredSizeInfo preferred_main_axis_sizes[3];
  LayoutUnit total_max_size_for_auto;
  bool has_auto_sized_box = false;
  for (int i = 0; i < 3; i++) {
    if (!nodes[i]) {
      continue;
    }
    preferred_main_axis_sizes[i] =
        EdgeMarginNodePreferredSize(nodes[i], available_logical_size, dir);
    // Tentatively set main sizes to the preferred ones. Any auto specified size
    // will be adjusted further below.
    final_main_axis_sizes[i] = preferred_main_axis_sizes[i].MaxLength();

    if (preferred_main_axis_sizes[i].IsAuto()) {
      has_auto_sized_box = true;
      total_max_size_for_auto += preferred_main_axis_sizes[i].MaxLength();
    }
  }

  if (has_auto_sized_box && !total_max_size_for_auto) {
    // There's no content in any of the auto-sized boxes to take up space. Make
    // sure that extra space is distributed evenly by giving them a non-zero max
    // content size (1) so that they get the same flex factor.
    for (auto& preferred_main_axis_size : preferred_main_axis_sizes) {
      if (preferred_main_axis_size.IsAuto()) {
        preferred_main_axis_size = PreferredSizeInfo(
            /*min_max=*/{LayoutUnit(1), LayoutUnit(1)},
            /*margin_sum=*/LayoutUnit(), /*is_auto=*/true);
      }
    }
  }

  if (nodes[CenterMarginBox]) {
    if (preferred_main_axis_sizes[CenterMarginBox].IsAuto()) {
      // To resolve auto center size, and to allow for center placement, resolve
      // for start and end separately multiplied by two. Figure out which one
      // results in a bigger size, and thus a smaller center size.
      //
      // The spec introduces an imaginary "AC" box, which is twice the larger of
      // start and end, but this needs to be done in two separate steps, in case
      // one of start and end has auto size, whereas the other doesn't (and
      // should therefore not be stretched).
      //
      // See https://drafts.csswg.org/css-page-3/#variable-auto-sizing
      PreferredSizeInfo ac_sizes_for_start[3] = {
          preferred_main_axis_sizes[CenterMarginBox], PreferredSizeInfo(),
          preferred_main_axis_sizes[StartMarginBox].Doubled()};
      PreferredSizeInfo ac_sizes_for_end[3] = {
          preferred_main_axis_sizes[CenterMarginBox], PreferredSizeInfo(),
          preferred_main_axis_sizes[EndMarginBox].Doubled()};

      LayoutUnit center_size1;
      LayoutUnit center_size2;
      LayoutUnit ignored_ac_size;
      ResolveTwoEdgeMarginBoxLengths(ac_sizes_for_start,
                                     available_main_axis_size, &center_size1,
                                     &ignored_ac_size);
      ResolveTwoEdgeMarginBoxLengths(ac_sizes_for_end, available_main_axis_size,
                                     &center_size2, &ignored_ac_size);
      final_main_axis_sizes[CenterMarginBox] =
          std::min(center_size1, center_size2);
    }
    // Any auto start or end should receive half of the space not used by
    // center.
    LayoutUnit side_space =
        available_main_axis_size - final_main_axis_sizes[CenterMarginBox];
    if (preferred_main_axis_sizes[StartMarginBox].IsAuto()) {
      final_main_axis_sizes[StartMarginBox] = side_space / 2;
    }
    if (preferred_main_axis_sizes[EndMarginBox].IsAuto()) {
      // If both start and end are auto, make sure that start+end is exactly
      // side_space (avoid rounding errors).
      final_main_axis_sizes[EndMarginBox] = side_space - side_space / 2;
    }
  } else {
    ResolveTwoEdgeMarginBoxLengths(preferred_main_axis_sizes,
                                   available_main_axis_size,
                                   &final_main_axis_sizes[StartMarginBox],
                                   &final_main_axis_sizes[EndMarginBox]);
  }

  // TODO(crbug.com/40341678): Honor min-width, max-width, min-height,
  // max-height.

  // Convert from margin-box to border-box lengths.
  for (int i = 0; i < 3; i++) {
    final_main_axis_sizes[i] -= preferred_main_axis_sizes[i].MarginSum();
    final_main_axis_sizes[i] = final_main_axis_sizes[i].ClampNegativeToZero();
  }
}

void PageContainerLayoutAlgorithm::ResolveTwoEdgeMarginBoxLengths(
    const PreferredSizeInfo preferred_main_axis_sizes[3],
    LayoutUnit available_main_axis_size,
    LayoutUnit* first_main_axis_size,
    LayoutUnit* second_main_axis_size) {
  // If the center box has non-auto main size, preferred_main_axis_sizes will
  // here simply be that of the start, center and end boxes.
  //
  // However, if center has auto size, the actual preferred sizes for auto is
  // moved to FirstResolvee, and the double of the preferred size for either
  // start or end can be found at SecondResolvee. In this case, this function
  // will be called twice, once for the double start box and once for the double
  // end box. Then the caller will decide which result to keep.
  DCHECK(!preferred_main_axis_sizes[NonResolvee].IsAuto());

  // First determine how much of the space is auto, to calculate bases for the
  // flex factor sum (min, max, or max minus min; see below), and how much space
  // is available for the auto-sized boxes.
  LayoutUnit available_main_axis_size_for_flex = available_main_axis_size;
  LayoutUnit total_auto_min_size;
  LayoutUnit total_auto_max_size;
  for (int i = 0; i < 3; i++) {
    if (preferred_main_axis_sizes[i].IsAuto()) {
      total_auto_min_size += preferred_main_axis_sizes[i].MinLength();
      total_auto_max_size += preferred_main_axis_sizes[i].MaxLength();
    } else {
      // Fixed-size box.
      available_main_axis_size_for_flex -=
          preferred_main_axis_sizes[i].Length();
    }
  }

  LayoutUnit flex_space;  // Additional space to distribute to auto-sized boxes.
  LayoutUnit unflexed_sizes[3];
  LayoutUnit flex_factors[3];
  if (available_main_axis_size_for_flex > total_auto_max_size) {
    flex_space = available_main_axis_size_for_flex - total_auto_max_size;
    // The sum of the max content lengths is less than available length. Each
    // box's flex factor is proportional to its max content length.
    for (int i = 0; i < 3; i++) {
      unflexed_sizes[i] = preferred_main_axis_sizes[i].MaxLength();
      flex_factors[i] = unflexed_sizes[i];
    }
  } else {
    flex_space = available_main_axis_size_for_flex - total_auto_min_size;
    for (int i = 0; i < 3; i++) {
      unflexed_sizes[i] = preferred_main_axis_sizes[i].MinLength();
    }
    if (flex_space > LayoutUnit()) {
      // The sum of the min content lengths is less than the available length
      // (whereas the sum of the *max* content lengths is not). Each box's flex
      // factor as proportional to its max content length minus min content
      // length,
      for (int i = 0; i < 3; i++) {
        flex_factors[i] = preferred_main_axis_sizes[i].MaxLength() -
                          preferred_main_axis_sizes[i].MinLength();
      }
    } else {
      // The sum of min sizes is larger than available size. Boxes will have to
      // shrink below their min content length to fit.
      for (int i = 0; i < 3; i++) {
        flex_factors[i] = preferred_main_axis_sizes[i].MinLength();
      }
    }
  }

  *first_main_axis_size = unflexed_sizes[FirstResolvee];
  *second_main_axis_size = unflexed_sizes[SecondResolvee];
  if (preferred_main_axis_sizes[FirstResolvee].IsAuto()) {
    if (preferred_main_axis_sizes[SecondResolvee].IsAuto()) {
      // Both have auto size.
      LayoutUnit total_flex =
          flex_factors[FirstResolvee] + flex_factors[SecondResolvee];
      if (total_flex > LayoutUnit()) {
        *first_main_axis_size +=
            flex_space * flex_factors[FirstResolvee] / total_flex;
      }
    } else {
      // Only first has auto size.
      *first_main_axis_size = available_main_axis_size - *second_main_axis_size;
    }
  }
  if (preferred_main_axis_sizes[SecondResolvee].IsAuto()) {
    // Second has auto size.
    *second_main_axis_size = available_main_axis_size - *first_main_axis_size;
  }
}

void PageContainerLayoutAlgorithm::LayoutEdgeMarginNode(
    const BlockNode& child,
    const PhysicalRect& edge_rect,
    LayoutUnit main_axis_size,
    EdgeMarginType edge_margin_type,
    EdgeAdjacency edge_adjacency,
    ProgressionDirection dir) {
  if (!child) {
    return;
  }

  ConstraintSpaceBuilder space_builder(GetConstraintSpace(),
                                       child.Style().GetWritingDirection(),
                                       /*is_new_fc=*/true);
  LogicalSize available_size =
      edge_rect.size.ConvertToLogical(Style().GetWritingMode());
  bool main_axis_is_inline =
      IsHorizontal(dir) == Style().IsHorizontalWritingMode();
  if (main_axis_is_inline) {
    available_size.inline_size = main_axis_size;
    space_builder.SetIsFixedInlineSize(true);
  } else {
    available_size.block_size = main_axis_size;
    space_builder.SetIsFixedBlockSize(true);
  }
  PrepareMarginBoxSpaceBuilder(available_size, &space_builder);
  ConstraintSpace child_space = space_builder.ToConstraintSpace();

  const LayoutResult* result = child.Layout(child_space);
  const auto& box_fragment =
      To<PhysicalBoxFragment>(result->GetPhysicalFragment());
  PhysicalBoxStrut physical_margins =
      ResolveMargins(child_space, child.Style(), box_fragment.Size(),
                     edge_rect.size, edge_adjacency);
  LayoutUnit main_axis_available_size;
  LayoutUnit main_axis_fragment_size;
  if (IsHorizontal(dir)) {
    main_axis_available_size = edge_rect.size.width;
    main_axis_fragment_size =
        box_fragment.Size().width + physical_margins.HorizontalSum();
  } else {
    main_axis_available_size = edge_rect.size.height;
    main_axis_fragment_size =
        box_fragment.Size().height + physical_margins.VerticalSum();
  }
  LayoutUnit main_axis_offset;
  switch (edge_margin_type) {
    case StartMarginBox:
      break;
    case CenterMarginBox:
      main_axis_offset =
          (main_axis_available_size - main_axis_fragment_size) / 2;
      break;
    case EndMarginBox:
      main_axis_offset = main_axis_available_size - main_axis_fragment_size;
      break;
  }
  PhysicalOffset offset = edge_rect.offset + physical_margins.Offset();
  if (IsHorizontal(dir)) {
    offset.left += main_axis_offset;
  } else {
    offset.top += main_axis_offset;
  }
  WritingModeConverter converter(Style().GetWritingDirection(),
                                 GetConstraintSpace().AvailableSize());
  LogicalOffset logical_offset =
      converter.ToLogical(offset, box_fragment.Size());
  container_builder_.AddResult(*result, logical_offset);
}

PhysicalBoxStrut PageContainerLayoutAlgorithm::ResolveMargins(
    const ConstraintSpace& child_space,
    const ComputedStyle& child_style,
    PhysicalSize child_size,
    PhysicalSize available_size,
    EdgeAdjacency edge_adjacency) const {
  PhysicalBoxStrut margins =
      ComputePhysicalMargins(child_style, available_size);

  // Auto margins are only considered when adjacent to one of the four edges of
  // a page. All other auto values resolve to 0.
  if (IsAtVerticalEdge(edge_adjacency)) {
    LayoutUnit additional_space =
        available_size.height - child_size.height - margins.VerticalSum();
    ResolveAutoMargins(child_style.MarginTop(), child_style.MarginBottom(),
                       additional_space.ClampNegativeToZero(), &margins.top,
                       &margins.bottom);
    LayoutUnit margin_box_size = child_size.height + margins.VerticalSum();
    LayoutUnit inequality = available_size.height - margin_box_size;
    if (inequality) {
      // Over-constrained. Solve the sizing equation my adjusting the margin
      // facing away from the center (which will normally move the box towards
      // the center).
      if (IsAtTopEdge(edge_adjacency)) {
        margins.top += inequality;
      } else {
        margins.bottom += inequality;
      }
    }
  }

  if (IsAtHorizontalEdge(edge_adjacency)) {
    LayoutUnit additional_space =
        available_size.width - child_size.width - margins.HorizontalSum();
    ResolveAutoMargins(child_style.MarginLeft(), child_style.MarginRight(),
                       additional_space.ClampNegativeToZero(), &margins.left,
                       &margins.right);
    LayoutUnit margin_box_size = child_size.width + margins.HorizontalSum();
    LayoutUnit inequality = available_size.width - margin_box_size;
    if (inequality) {
      // Over-constrained. Solve the sizing equation my adjusting the margin
      // facing away from the center (which will normally move the box towards
      // the center).
      if (IsAtLeftEdge(edge_adjacency)) {
        margins.left += inequality;
      } else {
        margins.right += inequality;
      }
    }
  }

  return margins;
}

}  // namespace blink
