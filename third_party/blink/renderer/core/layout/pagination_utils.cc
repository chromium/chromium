// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/pagination_utils.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_fragment_link.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

wtf_size_t PageCount(const LayoutView& view) {
  DCHECK(view.ShouldUsePrintingLayout());
  const auto& fragments = view.GetPhysicalFragment(0)->Children();
  return ClampTo<wtf_size_t>(fragments.size());
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
  const auto& fragments = layout_view.GetPhysicalFragment(0)->Children();
  CHECK_GE(fragments.size(), 1u);
  const PhysicalFragmentLink& page = fragments[page_number];
  DCHECK_EQ(page->GetBoxType(), PhysicalFragment::kPageContainer);
  return PhysicalRect(page.offset, page->Size());
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
