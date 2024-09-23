// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"

#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"

namespace blink {

FontHeight LogicalBoxFragment::BaselineMetrics(
    const LineBoxStrut& margins,
    FontBaseline baseline_type) const {
  const auto& fragment = GetPhysicalBoxFragment();
  const auto& style = fragment.Style();

  std::optional<LayoutUnit> baseline;
  switch (style.BaselineSource()) {
    case EBaselineSource::kAuto: {
      baseline = fragment.UseLastBaselineForInlineBaseline() ? LastBaseline()
                                                             : FirstBaseline();
      if (fragment.ForceInlineBaselineSynthesis()) {
        baseline = std::nullopt;
      }
      break;
    }
    case EBaselineSource::kFirst:
      baseline = FirstBaseline();
      break;
    case EBaselineSource::kLast:
      baseline = LastBaseline();
      break;
  }

  if (baseline) {
    FontHeight metrics = writing_direction_.IsFlippedLines()
                             ? FontHeight(BlockSize() - *baseline, *baseline)
                             : FontHeight(*baseline, BlockSize() - *baseline);

    // For replaced elements, inline-block elements, and inline-table elements,
    // the height is the height of their margin-box.
    // https://drafts.csswg.org/css2/visudet.html#line-height
    metrics.ascent += margins.line_over;
    metrics.descent += margins.line_under;

    return metrics;
  }

  const auto SynthesizeMetrics = [&](LayoutUnit size) -> FontHeight {
    return baseline_type == kAlphabeticBaseline
               ? FontHeight(size, LayoutUnit())
               : FontHeight(size - size / 2, size / 2);
  };

  // The baseline was not found, synthesize it off the appropriate edge.
  switch (style.InlineBlockBaselineEdge()) {
    case EInlineBlockBaselineEdge::kMarginBox: {
      const LayoutUnit margin_size = BlockSize() + margins.BlockSum();
      return SynthesizeMetrics(margin_size);
    }
    case EInlineBlockBaselineEdge::kBorderBox: {
      FontHeight metrics = SynthesizeMetrics(BlockSize());
      metrics.ascent += margins.line_over;
      metrics.descent += margins.line_under;
      return metrics;
    }
    case EInlineBlockBaselineEdge::kContentBox: {
      const LineBoxStrut border_scrollbar_padding(
          Borders() + Scrollbar() + Padding(),
          writing_direction_.IsFlippedLines());
      const LayoutUnit content_size =
          (BlockSize() - border_scrollbar_padding.BlockSum())
              .ClampNegativeToZero();
      FontHeight metrics = SynthesizeMetrics(content_size);
      metrics.ascent += margins.line_over + border_scrollbar_padding.line_over;
      metrics.descent +=
          margins.line_under + border_scrollbar_padding.line_under;
      return metrics;
    }
  }
}

LayoutUnit LogicalBoxFragment::BlockEndScrollableOverflow() const {
  WritingModeConverter converter(writing_direction_, physical_fragment_.Size());
  LogicalRect overflow =
      converter.ToLogical(GetPhysicalBoxFragment().ScrollableOverflow());
  return overflow.BlockEndOffset();
}

}  // namespace blink
