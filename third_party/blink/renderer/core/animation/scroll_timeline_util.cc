// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"

#include "third_party/blink/renderer/bindings/core/v8/double_or_scroll_timeline_auto_keyword.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"

namespace blink {

namespace scroll_timeline_util {

std::unique_ptr<CompositorScrollTimeline> ToCompositorScrollTimeline(
    AnimationTimeline* timeline) {
  if (!timeline || timeline->IsDocumentTimeline())
    return nullptr;

  ScrollTimeline* scroll_timeline = ToScrollTimeline(timeline);
  Node* scroll_source = scroll_timeline->ResolvedScrollSource();
  base::Optional<CompositorElementId> element_id =
      GetCompositorScrollElementId(scroll_source);

  DoubleOrScrollTimelineAutoKeyword time_range;
  scroll_timeline->timeRange(time_range);
  // TODO(smcgruer): Handle 'auto' time range value.
  DCHECK(time_range.IsDouble());

  LayoutBox* box = scroll_source ? scroll_source->GetLayoutBox() : nullptr;

  CompositorScrollTimeline::ScrollDirection orientation = ConvertOrientation(
      scroll_timeline->GetOrientation(), box ? box->Style() : nullptr);

  base::Optional<double> start_scroll_offset;
  base::Optional<double> end_scroll_offset;
  if (box) {
    double current_offset;
    double max_offset;
    scroll_timeline->GetCurrentAndMaxOffset(box, current_offset, max_offset);

    double resolved_start_scroll_offset = 0;
    double resolved_end_scroll_offset = max_offset;
    scroll_timeline->ResolveScrollStartAndEnd(box, max_offset,
                                              resolved_start_scroll_offset,
                                              resolved_end_scroll_offset);
    start_scroll_offset = resolved_start_scroll_offset;
    end_scroll_offset = resolved_end_scroll_offset;
  }

  return std::make_unique<CompositorScrollTimeline>(
      element_id, orientation, start_scroll_offset, end_scroll_offset,
      time_range.GetAsDouble(), scroll_timeline->GetFillMode());
}

base::Optional<CompositorElementId> GetCompositorScrollElementId(
    const Node* node) {
  if (!node || !node->GetLayoutObject() ||
      !node->GetLayoutObject()->FirstFragment().PaintProperties()) {
    return base::nullopt;
  }
  return CompositorElementIdFromUniqueObjectId(
      node->GetLayoutObject()->UniqueId(),
      CompositorElementIdNamespace::kScroll);
}

// The compositor does not know about writing modes, so we have to convert the
// web concepts of 'block' and 'inline' direction into absolute vertical or
// horizontal directions.
CompositorScrollTimeline::ScrollDirection ConvertOrientation(
    ScrollTimeline::ScrollDirection orientation,
    const ComputedStyle* style) {
  // Easy cases; physical is always physical.
  if (orientation == ScrollTimeline::Horizontal)
    return CompositorScrollTimeline::ScrollRight;
  if (orientation == ScrollTimeline::Vertical)
    return CompositorScrollTimeline::ScrollDown;

  // Harder cases; first work out which axis is which, and then for each check
  // which edge we start at.

  // writing-mode: horizontal-tb
  bool is_horizontal_writing_mode =
      style ? style->IsHorizontalWritingMode() : true;
  // writing-mode: vertical-lr
  bool is_flipped_lines_writing_mode =
      style ? style->IsFlippedLinesWritingMode() : false;
  // direction: ltr;
  bool is_ltr_direction = style ? style->IsLeftToRightDirection() : true;

  if (orientation == ScrollTimeline::Block) {
    if (is_horizontal_writing_mode) {
      // For horizontal writing mode, block is vertical. The starting edge is
      // always the top.
      return CompositorScrollTimeline::ScrollDown;
    }
    // For vertical writing mode, the block axis is horizontal. The starting
    // edge depends on if we are lr or rl.
    return is_flipped_lines_writing_mode ? CompositorScrollTimeline::ScrollRight
                                         : CompositorScrollTimeline::ScrollLeft;
  }

  DCHECK_EQ(orientation, ScrollTimeline::Inline);
  if (is_horizontal_writing_mode) {
    // For horizontal writing mode, inline is horizontal. The starting edge
    // depends on the directionality.
    return is_ltr_direction ? CompositorScrollTimeline::ScrollRight
                            : CompositorScrollTimeline::ScrollLeft;
  }
  // For vertical writing mode, inline is vertical. The starting edge still
  // depends on the directionality; whether it is vertical-lr or vertical-rl
  // does not matter.
  return is_ltr_direction ? CompositorScrollTimeline::ScrollDown
                          : CompositorScrollTimeline::ScrollUp;
}

}  // namespace scroll_timeline_util

}  // namespace blink
