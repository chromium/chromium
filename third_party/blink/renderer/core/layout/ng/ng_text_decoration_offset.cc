// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_text_decoration_offset.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_baseline.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/font_metrics.h"

namespace blink {

int NGTextDecorationOffset::ComputeUnderlineOffsetForUnder(
    float text_decoration_thickness,
    FontVerticalPositionType position_type) const {
  LayoutUnit offset = LayoutUnit::Max();
  const ComputedStyle& style = text_style_;
  FontBaseline baseline_type = style.GetFontBaseline();

  if (decorating_box_) {
    NGBaselineRequest baseline_request = {
        NGBaselineAlgorithmType::kAtomicInline,
        FontBaseline::kIdeographicBaseline};

    if (base::Optional<LayoutUnit> baseline =
            decorating_box_->Baseline(baseline_request))
      offset = *baseline;
  }

  if (offset == LayoutUnit::Max()) {
    // TODO(layout-dev): How do we compute the baseline offset with a
    // decorating_box?
    const SimpleFontData* font_data = style.GetFont().PrimaryFont();
    if (!font_data)
      return 0;
    offset = font_data->GetFontMetrics().Ascent(baseline_type) -
             font_data->VerticalPosition(position_type, baseline_type);
  }

  // Compute offset to the farthest position of the decorating box.
  // TODO(layout-dev): This does not take farthest offset within the decorating
  // box into account, only the position within this text fragment.
  int offset_int = offset.Floor();

  // Gaps are not needed for TextTop because it generally has internal
  // leadings.
  if (position_type == FontVerticalPositionType::TextTop)
    return offset_int;
  return !IsLineOverSide(position_type) ? offset_int + 1 : offset_int - 1;
}

}  // namespace blink
