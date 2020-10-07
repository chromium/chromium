// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

FontHeight NGBoxFragment::BaselineMetrics(const NGLineBoxStrut& margins,
                                          FontBaseline baseline_type) const {
  DCHECK(physical_fragment_.IsAtomicInline() ||
         physical_fragment_.IsListMarker());

  // For checkbox and radio controls, we always use the border edge instead of
  // the margin edge.
  if (physical_fragment_.Style().IsCheckboxOrRadioPart()) {
    return FontHeight(margins.line_over + BlockSize(), margins.line_under);
  }

  if (const base::Optional<LayoutUnit> baseline = Baseline()) {
    FontHeight metrics = IsFlippedLinesWritingMode(writing_mode_)
                             ? FontHeight(BlockSize() - *baseline, *baseline)
                             : FontHeight(*baseline, BlockSize() - *baseline);

    // For replaced elements, inline-block elements, and inline-table elements,
    // the height is the height of their margin-box.
    // https://drafts.csswg.org/css2/visudet.html#line-height
    metrics.ascent += margins.line_over;
    metrics.descent += margins.line_under;

    return metrics;
  }

  // The baseline type was not found. This is either this box should synthesize
  // box-baseline without propagating from children, or caller forgot to add
  // baseline requests to constraint space when it called Layout().
  LayoutUnit block_size = BlockSize() + margins.BlockSum();

  if (baseline_type == kAlphabeticBaseline)
    return FontHeight(block_size, LayoutUnit());
  return FontHeight(block_size - block_size / 2, block_size / 2);
}

}  // namespace blink
