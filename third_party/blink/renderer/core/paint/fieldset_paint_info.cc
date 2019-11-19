// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/fieldset_paint_info.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

FieldsetPaintInfo::FieldsetPaintInfo(const ComputedStyle& fieldset_style,
                                     const PhysicalSize& fieldset_size,
                                     const LayoutRectOutsets& fieldset_borders,
                                     const PhysicalRect& legend_border_box) {
  if (fieldset_style.IsHorizontalWritingMode()) {
    // horizontal-tb
    LayoutUnit legend_size = legend_border_box.size.height;
    LayoutUnit border_size = fieldset_borders.Top();
    LayoutUnit legend_excess_size = legend_size - border_size;
    if (legend_excess_size > LayoutUnit())
      border_outsets.SetTop(legend_excess_size / 2);
    legend_cutout_rect = PhysicalRect(legend_border_box.X(), LayoutUnit(),
                                      legend_border_box.Width(),
                                      std::max(legend_size, border_size));
  } else {
    LayoutUnit legend_size = legend_border_box.Width();
    LayoutUnit border_size;
    if (fieldset_style.IsFlippedBlocksWritingMode()) {
      // vertical-rl
      border_size = fieldset_borders.Right();
      LayoutUnit legend_excess_size = legend_size - border_size;
      if (legend_excess_size > LayoutUnit())
        border_outsets.SetRight(legend_excess_size / 2);
    } else {
      // vertical-lr
      border_size = fieldset_borders.Left();
      LayoutUnit legend_excess_size = legend_size - border_size;
      if (legend_excess_size > LayoutUnit())
        border_outsets.SetLeft(legend_excess_size / 2);
    }
    LayoutUnit legend_total_block_size = std::max(legend_size, border_size);
    legend_cutout_rect =
        PhysicalRect(LayoutUnit(), legend_border_box.offset.top,
                     legend_total_block_size, legend_border_box.size.height);
    if (fieldset_style.IsFlippedBlocksWritingMode()) {
      // Offset cutout to right fieldset edge for vertical-rl
      LayoutUnit clip_x = fieldset_size.width - legend_total_block_size;
      legend_cutout_rect.offset.left += clip_x;
    }
  }
}

}  // namespace blink
