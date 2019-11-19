// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_height_metrics.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

NGLineHeightMetrics NGBoxFragment::BaselineMetricsWithoutSynthesize(
    const NGBaselineRequest& request) const {
  // For "leaf" theme objects, let the theme decide what the baseline position
  // is. The theme baseline wins over the propagated baselines.
  const auto& physical_fragment = To<NGPhysicalBoxFragment>(physical_fragment_);
  DCHECK(physical_fragment_.GetLayoutObject());
  const LayoutBox& layout_box =
      ToLayoutBox(*physical_fragment_.GetLayoutObject());
  const ComputedStyle& style = physical_fragment.Style();
  if (style.HasEffectiveAppearance() &&
      !LayoutTheme::GetTheme().IsControlContainer(
          style.EffectiveAppearance())) {
    return NGLineHeightMetrics(
        BlockSize() + layout_box.MarginOver() +
            LayoutTheme::GetTheme().BaselinePositionAdjustment(style),
        layout_box.MarginUnder());
  }

  // Check if we have a propagated baseline.
  if (base::Optional<LayoutUnit> baseline =
          physical_fragment.Baseline(request)) {
    LayoutUnit ascent = *baseline;
    LayoutUnit descent = BlockSize() - ascent;

    // For replaced elements, inline-block elements, and inline-table
    // elements, the height is the height of their margin box.
    // https://drafts.csswg.org/css2/visudet.html#line-height
    if (layout_box.IsAtomicInlineLevel()) {
      ascent += layout_box.MarginOver();
      descent += layout_box.MarginUnder();
    }

    return NGLineHeightMetrics(ascent, descent);
  }

  return NGLineHeightMetrics();
}

NGLineHeightMetrics NGBoxFragment::BaselineMetrics(
    const NGBaselineRequest& request,
    const NGConstraintSpace& constraint_space) const {
  // Try to compute the baseline if the writing-modes are the same.
  if (constraint_space.GetWritingMode() == GetWritingMode()) {
    NGLineHeightMetrics metrics = BaselineMetricsWithoutSynthesize(request);
    if (!metrics.IsEmpty())
      return metrics;
  }

  // The baseline type was not found. This is either this box should synthesize
  // box-baseline without propagating from children, or caller forgot to add
  // baseline requests to constraint space when it called Layout().
  LayoutUnit block_size = BlockSize();

  // If atomic inline, use the margin box. See above.
  const auto& physical_fragment = To<NGPhysicalBoxFragment>(physical_fragment_);
  DCHECK(physical_fragment_.GetLayoutObject());
  const LayoutBox& layout_box =
      ToLayoutBox(*physical_fragment_.GetLayoutObject());
  if (layout_box.IsAtomicInlineLevel()) {
    bool is_parallel_writing_mode =
        IsParallelWritingMode(constraint_space.GetWritingMode(),
                              physical_fragment.Style().GetWritingMode());
    if (is_parallel_writing_mode)
      block_size += layout_box.MarginLogicalHeight();
    else
      block_size += layout_box.MarginLogicalWidth();
  }

  if (request.BaselineType() == kAlphabeticBaseline)
    return NGLineHeightMetrics(block_size, LayoutUnit());
  return NGLineHeightMetrics(block_size - block_size / 2, block_size / 2);
}

}  // namespace blink
