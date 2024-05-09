// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/pagination_utils.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_fragment_link.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

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

wtf_size_t PageCount(const LayoutView& view) {
  DCHECK(view.ShouldUsePrintingLayout());
  const auto& fragments = view.GetPhysicalFragment(0)->Children();
  return ClampTo<wtf_size_t>(fragments.size());
}

const PhysicalBoxFragment* GetPageContainer(const LayoutView& view,
                                            wtf_size_t page_number) {
  if (!view.PhysicalFragmentCount()) {
    return nullptr;
  }
  const auto& pages = view.GetPhysicalFragment(0)->Children();
  if (page_number >= pages.size()) {
    return nullptr;
  }
  const auto* child = To<PhysicalBoxFragment>(pages[page_number].get());
  if (child->GetBoxType() != PhysicalFragment::kPageContainer) {
    // Not paginated, at least not yet.
    return nullptr;
  }
  return child;
}

const PhysicalBoxFragment* GetPageArea(const LayoutView& view,
                                       wtf_size_t page_number) {
  const auto* page_container = GetPageContainer(view, page_number);
  if (!page_container) {
    return nullptr;
  }
  return &GetPageArea(GetPageBorderBox(*page_container));
}

const PhysicalBoxFragment& GetPageBorderBox(
    const PhysicalBoxFragment& page_container) {
  DCHECK_EQ(page_container.GetBoxType(), PhysicalFragment::kPageContainer);
  for (const auto& child : page_container.Children()) {
    if (child->GetBoxType() == PhysicalFragment::kPageBorderBox) {
      return *To<PhysicalBoxFragment>(child.get());
    }
  }
  // A page container will never be laid out without a page border box child.
  NOTREACHED_NORETURN();
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
                                     wtf_size_t page_number) {
  return StitchedPageContentRect(*GetPageContainer(layout_view, page_number));
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

}  // namespace blink
