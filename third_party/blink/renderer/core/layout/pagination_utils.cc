// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/pagination_utils.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_fragment_link.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

float CalculateOverflowShrinkForPrinting(const LayoutView& view,
                                         float maximum_shrink_factor) {
  float overall_scale_factor = 1.0;
  for (const PhysicalFragmentLink& link :
       view.GetPhysicalFragment(0)->Children()) {
    const auto& page = To<PhysicalBoxFragment>(*link);
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
    overall_scale_factor = std::max(overall_scale_factor, page_scale_factor);
    if (overall_scale_factor >= maximum_shrink_factor) {
      return maximum_shrink_factor;
    }
  }

  return overall_scale_factor;
}

}  // namespace blink
