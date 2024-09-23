// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/pagination_utils.h"

#include "printing/mojom/print.mojom-blink.h"
#include "third_party/blink/public/web/web_print_page_description.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/geometry/fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_fragment_link.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

bool ShouldCenterPageOnPaper(const WebPrintParams& params) {
  if (params.print_scaling_option ==
      printing::mojom::blink::PrintScalingOption::kCenterShrinkToFitPaper) {
    return true;
  }
  DCHECK(params.print_scaling_option ==
         printing::mojom::blink::PrintScalingOption::kSourceSize);
  return false;
}

PhysicalSize PageBoxDefaultSize(const Document& document) {
  const WebPrintParams& params = document.GetFrame()->GetPrintParams();
  return PhysicalSize::FromSizeFRound(params.default_page_description.size);
}

LogicalSize PageBoxDefaultSizeWithSourceOrientation(const Document& document,
                                                    const ComputedStyle& style,
                                                    LogicalSize layout_size) {
  DCHECK(ShouldCenterPageOnPaper(document.GetFrame()->GetPrintParams()));
  LogicalSize target_size =
      PageBoxDefaultSize(document).ConvertToLogical(style.GetWritingMode());
  if (layout_size.inline_size != layout_size.block_size &&
      (target_size.inline_size > target_size.block_size) !=
          (layout_size.inline_size > layout_size.block_size)) {
    // Match orientation requested / implied by CSS.
    std::swap(target_size.inline_size, target_size.block_size);
  }
  return target_size;
}

float TargetShrinkScaleFactor(LogicalSize target_size,
                              LogicalSize source_size) {
  if (source_size.IsEmpty()) {
    return 1.0f;
  }
  float inline_scale =
      target_size.inline_size.ToFloat() / source_size.inline_size.ToFloat();
  float block_scale =
      target_size.block_size.ToFloat() / source_size.block_size.ToFloat();
  return std::min(1.0f, std::min(inline_scale, block_scale));
}

wtf_size_t PageNumberFromPageArea(const PhysicalBoxFragment& page_area) {
  DCHECK_EQ(page_area.GetBoxType(), PhysicalFragment::kPageArea);
  if (const BlockBreakToken* break_token = page_area.GetBreakToken()) {
    return break_token->SequenceNumber();
  }
  const LayoutView& view = *page_area.GetDocument().GetLayoutView();
  DCHECK_GE(PageCount(view), 1u);
  return PageCount(view) - 1;
}

}  // anonymous namespace

void SetUpSpaceBuilderForPageBox(LogicalSize available_size,
                                 ConstraintSpaceBuilder* builder) {
  builder->SetAvailableSize(available_size);
  builder->SetPercentageResolutionSize(available_size);
  builder->SetInlineAutoBehavior(AutoSizeBehavior::kStretchImplicit);
  builder->SetBlockAutoBehavior(AutoSizeBehavior::kStretchImplicit);
  builder->SetDecorationPercentageResolutionType(
      DecorationPercentageResolutionType::kContainingBlockSize);
}

LogicalSize DesiredPageContainingBlockSize(const Document& document,
                                           const ComputedStyle& style) {
  PhysicalSize layout_size = PageBoxDefaultSize(document);
  switch (style.GetPageSizeType()) {
    case PageSizeType::kAuto:
      break;
    case PageSizeType::kLandscape:
      if (layout_size.width < layout_size.height) {
        std::swap(layout_size.width, layout_size.height);
      }
      break;
    case PageSizeType::kPortrait:
      if (layout_size.width > layout_size.height) {
        std::swap(layout_size.width, layout_size.height);
      }
      break;
    case PageSizeType::kFixed: {
      auto css_size = PhysicalSize::FromSizeFRound(style.PageSize());
      if (document.GetFrame()->GetPrintParams().ignore_page_size) {
        // Keep the page size, but match orientation.
        if ((css_size.width > css_size.height) !=
            (layout_size.width > layout_size.height)) {
          std::swap(layout_size.width, layout_size.height);
        }
        break;
      }
      layout_size = css_size;
      break;
    }
  }

  return layout_size.ConvertToLogical(style.GetWritingMode());
}

void ResolvePageBoxGeometry(const BlockNode& page_box,
                            LogicalSize page_containing_block_size,
                            FragmentGeometry* geometry,
                            BoxStrut* margins) {
  const ComputedStyle& style = page_box.Style();
  ConstraintSpaceBuilder space_builder(style.GetWritingMode(),
                                       style.GetWritingDirection(),
                                       /* is_new_fc */ true);
  SetUpSpaceBuilderForPageBox(page_containing_block_size, &space_builder);
  ConstraintSpace space = space_builder.ToConstraintSpace();
  *geometry = CalculateInitialFragmentGeometry(space, page_box,
                                               /*BlockBreakToken=*/nullptr);

  if (!margins) {
    return;
  }

  *margins = ComputeMarginsForSelf(space, style);

  // Resolve any auto margins. Note that this may result in negative margins, if
  // the specified width/height is larger than the specified containing block
  // size (the 'size' property). See
  // https://github.com/w3c/csswg-drafts/issues/8508 for discussion around
  // negative page margins in general.
  LayoutUnit additional_inline_space =
      space.AvailableSize().inline_size -
      (geometry->border_box_size.inline_size + margins->InlineSum());
  LayoutUnit additional_block_space =
      space.AvailableSize().block_size -
      (geometry->border_box_size.block_size + margins->BlockSum());
  ResolveAutoMargins(style.MarginInlineStart(), style.MarginInlineEnd(),
                     style.MarginBlockStart(), style.MarginBlockEnd(),
                     additional_inline_space, additional_block_space, margins);
}

PhysicalSize CalculateInitialContainingBlockSizeForPagination(
    Document& document) {
  const LayoutView& layout_view = *document.GetLayoutView();
  const ComputedStyle* page_style;
  // The initial containing block is the size of the first page area.
  if (const PhysicalBoxFragment* first_page =
          GetPageContainer(layout_view, 0)) {
    // We have already laid out. Grab the page style off the first page
    // fragment. It may have been adjusted due to named pages or unusable sizes
    // requested, which means that recomputing style here would not always give
    // the correct results.
    page_style = &first_page->Style();
  } else {
    page_style =
        document.GetStyleResolver().StyleForPage(0, /*page_name=*/g_null_atom);
  }

  // Simply reading out the size of the page container fragment (if it exists at
  // all) won't do, since we don't know if page scaling has been accounted for
  // or not at this point. Note that we may not even have created the first page
  // yet. This function is called before entering layout, so that viewport sizes
  // (to resolve viewport units) are set up before entering layout (and, after
  // layout, the sizes may need to be adjusted, if the initial estimate turned
  // out to be wrong). Create a temporary node and resolve the size.
  auto* page_box = LayoutBlockFlow::CreateAnonymous(&document, page_style);
  BlockNode temporary_page_node(page_box);

  FragmentGeometry geometry;
  LogicalSize containing_block_size =
      DesiredPageContainingBlockSize(document, *page_style);
  ResolvePageBoxGeometry(temporary_page_node, containing_block_size, &geometry);
  LogicalSize logical_size = ShrinkLogicalSize(
      geometry.border_box_size, geometry.border + geometry.padding);

  // Note: Don't get the writing mode directly from the LayoutView, since that
  // one is untrustworthy unless we have entered layout (which we might not have
  // at this point). See StyleResolver::StyleForViewport() and how it's called.
  WritingMode writing_mode = page_style->GetWritingMode();

  // So long, and thanks for all the size.
  page_box->Destroy();

  return ToPhysicalSize(logical_size, writing_mode) *
         layout_view.PaginationScaleFactor();
}

float TargetScaleForPage(const PhysicalBoxFragment& page_container) {
  DCHECK_EQ(page_container.GetBoxType(), PhysicalFragment::kPageContainer);
  const Document& document = page_container.GetDocument();
  const LayoutView& layout_view = *document.GetLayoutView();
  // Print parameters may set a scale factor, and layout may also use a larger
  // viewport size in order to fit more unbreakable content in the inline
  // direction.
  float layout_scale = 1.f / layout_view.PaginationScaleFactor();
  if (!ShouldCenterPageOnPaper(document.GetFrame()->GetPrintParams())) {
    return layout_scale;
  }

  // The source margin box size isn't stored anywhere, so it needs to be
  // recomputed now.
  BlockNode page_node(To<LayoutBox>(page_container.GetMutableLayoutObject()));
  const ComputedStyle& style = page_node.Style();
  FragmentGeometry geometry;
  BoxStrut margins;
  ResolvePageBoxGeometry(page_node,
                         DesiredPageContainingBlockSize(document, style),
                         &geometry, &margins);
  LogicalSize source_size = geometry.border_box_size + margins;
  LogicalSize target_size =
      page_container.Size().ConvertToLogical(style.GetWritingMode());

  return layout_scale * TargetShrinkScaleFactor(target_size, source_size);
}

LogicalSize FittedPageContainerSize(const Document& document,
                                    const ComputedStyle& style,
                                    LogicalSize source_margin_box_size) {
  if (!ShouldCenterPageOnPaper(document.GetFrame()->GetPrintParams())) {
    return source_margin_box_size;
  }

  // The target page size is fixed. This happens when printing to an actual
  // printer, whose page size is obviously confined to the size of the paper
  // sheets in the printer. Only honor orientation.
  return PageBoxDefaultSizeWithSourceOrientation(document, style,
                                                 source_margin_box_size);
}

LogicalRect TargetPageBorderBoxLogicalRect(
    const Document& document,
    const ComputedStyle& style,
    const LogicalSize& source_margin_box_size,
    const BoxStrut& margins) {
  LogicalSize source_border_box_size(
      source_margin_box_size.inline_size - margins.InlineSum(),
      source_margin_box_size.block_size - margins.BlockSum());
  LogicalRect rect(LogicalOffset(margins.inline_start, margins.block_start),
                   source_border_box_size);

  if (!ShouldCenterPageOnPaper(document.GetFrame()->GetPrintParams())) {
    return rect;
  }

  LogicalSize target_size = PageBoxDefaultSizeWithSourceOrientation(
      document, style, source_margin_box_size);

  float scale = TargetShrinkScaleFactor(target_size, source_margin_box_size);

  rect.offset.inline_offset =
      LayoutUnit(rect.offset.inline_offset.ToFloat() * scale +
                 (target_size.inline_size.ToFloat() -
                  source_margin_box_size.inline_size.ToFloat() * scale) /
                     2);
  rect.offset.block_offset =
      LayoutUnit(rect.offset.block_offset.ToFloat() * scale +
                 (target_size.block_size.ToFloat() -
                  source_margin_box_size.block_size.ToFloat() * scale) /
                     2);
  rect.size.inline_size = LayoutUnit(rect.size.inline_size.ToFloat() * scale);
  rect.size.block_size = LayoutUnit(rect.size.block_size.ToFloat() * scale);

  return rect;
}

wtf_size_t PageCount(const LayoutView& view) {
  DCHECK(view.ShouldUsePaginatedLayout());
  const auto& fragments = view.GetPhysicalFragment(0)->Children();
  return ClampTo<wtf_size_t>(fragments.size());
}

const PhysicalBoxFragment* GetPageContainer(const LayoutView& view,
                                            wtf_size_t page_index) {
  if (!view.PhysicalFragmentCount()) {
    return nullptr;
  }
  const auto& pages = view.GetPhysicalFragment(0)->Children();
  if (page_index >= pages.size()) {
    return nullptr;
  }
  const auto* child = To<PhysicalBoxFragment>(pages[page_index].get());
  if (child->GetBoxType() != PhysicalFragment::kPageContainer) {
    // Not paginated, at least not yet.
    return nullptr;
  }
  return child;
}

const PhysicalBoxFragment* GetPageArea(const LayoutView& view,
                                       wtf_size_t page_index) {
  const auto* page_container = GetPageContainer(view, page_index);
  if (!page_container) {
    return nullptr;
  }
  return &GetPageArea(GetPageBorderBox(*page_container));
}

const PhysicalFragmentLink& GetPageBorderBoxLink(
    const PhysicalBoxFragment& page_container) {
  DCHECK_EQ(page_container.GetBoxType(), PhysicalFragment::kPageContainer);
  for (const auto& child : page_container.Children()) {
    if (child->GetBoxType() == PhysicalFragment::kPageBorderBox) {
      return child;
    }
  }
  // A page container will never be laid out without a page border box child.
  NOTREACHED();
}

const PhysicalBoxFragment& GetPageBorderBox(
    const PhysicalBoxFragment& page_container) {
  return *To<PhysicalBoxFragment>(GetPageBorderBoxLink(page_container).get());
}

const PhysicalBoxFragment& GetPageArea(
    const PhysicalBoxFragment& page_border_box) {
  DCHECK_EQ(page_border_box.GetBoxType(), PhysicalFragment::kPageBorderBox);
  DCHECK_EQ(page_border_box.Children().size(), 1u);
  const auto& page_area =
      *DynamicTo<PhysicalBoxFragment>(page_border_box.Children()[0].get());
  DCHECK_EQ(page_area.GetBoxType(), PhysicalFragment::kPageArea);
  return page_area;
}

PhysicalRect StitchedPageContentRect(const LayoutView& layout_view,
                                     wtf_size_t page_index) {
  return StitchedPageContentRect(*GetPageContainer(layout_view, page_index));
}

PhysicalRect StitchedPageContentRect(
    const PhysicalBoxFragment& page_container) {
  DCHECK_EQ(page_container.GetBoxType(), PhysicalFragment::kPageContainer);
  const PhysicalBoxFragment& page_border_box = GetPageBorderBox(page_container);
  const PhysicalBoxFragment& page_area = GetPageArea(page_border_box);
  PhysicalRect physical_page_rect = page_area.LocalRect();

  if (const BlockBreakToken* previous_break_token =
          FindPreviousBreakTokenForPageArea(page_area)) {
    LayoutUnit consumed_block_size = previous_break_token->ConsumedBlockSize();
    WritingMode writing_mode = page_container.Style().GetWritingMode();
    if (writing_mode == WritingMode::kVerticalRl) {
      const LayoutView& view = *page_container.GetDocument().GetLayoutView();
      const PhysicalBoxFragment& first_page_area = *GetPageArea(view, 0);
      physical_page_rect.offset.left += first_page_area.Size().width;
      physical_page_rect.offset.left -=
          consumed_block_size + page_area.Size().width;
    } else if (writing_mode == WritingMode::kVerticalLr) {
      physical_page_rect.offset.left += consumed_block_size;
    } else {
      physical_page_rect.offset.top += consumed_block_size;
    }
  }

  return physical_page_rect;
}

const BlockBreakToken* FindPreviousBreakTokenForPageArea(
    const PhysicalBoxFragment& page_area) {
  DCHECK_EQ(page_area.GetBoxType(), PhysicalFragment::kPageArea);
  wtf_size_t page_number = PageNumberFromPageArea(page_area);
  if (page_number == 0) {
    return nullptr;
  }
  const LayoutView& view = *page_area.GetDocument().GetLayoutView();
  return GetPageArea(view, page_number - 1)->GetBreakToken();
}

float CalculateOverflowShrinkForPrinting(const LayoutView& view,
                                         float maximum_shrink_factor) {
  float overall_scale_factor = 1.0;
  for (const PhysicalFragmentLink& link :
       view.GetPhysicalFragment(0)->Children()) {
    const auto& page_container = To<PhysicalBoxFragment>(*link);
    for (const PhysicalFragmentLink& child : page_container.Children()) {
      if (child->GetBoxType() == PhysicalFragment::kPageBorderBox) {
        const auto& page = *To<PhysicalBoxFragment>(child->Children()[0].get());
        // Check the inline axis overflow on each individual page, to find the
        // largest relative overflow.
        float page_scale_factor;
        if (view.StyleRef().IsHorizontalWritingMode()) {
          page_scale_factor = page.ScrollableOverflow().Right().ToFloat() /
                              page.Size().width.ToFloat();
        } else {
          page_scale_factor = page.ScrollableOverflow().Bottom().ToFloat() /
                              page.Size().height.ToFloat();
        }
        overall_scale_factor =
            std::max(overall_scale_factor, page_scale_factor);
        break;
      }
    }

    if (overall_scale_factor >= maximum_shrink_factor) {
      return maximum_shrink_factor;
    }
  }

  return overall_scale_factor;
}

WebPrintPageDescription GetPageDescriptionFromLayout(const Document& document,
                                                     wtf_size_t page_number) {
  const PhysicalBoxFragment& page_container =
      *GetPageContainer(*document.GetLayoutView(), page_number);
  const ComputedStyle& style = page_container.Style();
  const PhysicalFragmentLink& border_box = GetPageBorderBoxLink(page_container);
  float scale = TargetScaleForPage(page_container);
  PhysicalRect page_border_box_rect(border_box.offset,
                                    border_box->Size() * scale);

  PhysicalBoxStrut insets(page_container.Size(), page_border_box_rect);

  // Go through all page margin boxes, and see which page edges they intersect
  // with. Set margins to zero for those edges, to suppress browser-generated
  // headers and footers, so that they don't overlap with the page margin boxes.
  PhysicalRect top_edge_rect(LayoutUnit(), LayoutUnit(),
                             page_container.Size().width, insets.top);
  PhysicalRect right_edge_rect(insets.left + page_border_box_rect.Width(),
                               LayoutUnit(), insets.right,
                               page_container.Size().height);
  PhysicalRect bottom_edge_rect(LayoutUnit(),
                                insets.top + page_border_box_rect.Height(),
                                page_container.Size().width, insets.bottom);
  PhysicalRect left_edge_rect(LayoutUnit(), LayoutUnit(), insets.left,
                              page_container.Size().height);
  for (const PhysicalFragmentLink& child_link : page_container.Children()) {
    if (child_link->GetBoxType() != PhysicalFragment::kPageMargin) {
      continue;
    }
    PhysicalRect box_rect(child_link.offset, child_link->Size());
    if (box_rect.Intersects(top_edge_rect)) {
      insets.top = LayoutUnit();
    }
    if (box_rect.Intersects(right_edge_rect)) {
      insets.right = LayoutUnit();
    }
    if (box_rect.Intersects(bottom_edge_rect)) {
      insets.bottom = LayoutUnit();
    }
    if (box_rect.Intersects(left_edge_rect)) {
      insets.left = LayoutUnit();
    }
  }

  WebPrintPageDescription description(gfx::SizeF(page_container.Size()));
  description.margin_top = insets.top.ToFloat();
  description.margin_right = insets.right.ToFloat();
  description.margin_bottom = insets.bottom.ToFloat();
  description.margin_left = insets.left.ToFloat();

  description.page_size_type = style.GetPageSizeType();
  description.orientation = style.GetPageOrientation();

  return description;
}

}  // namespace blink
