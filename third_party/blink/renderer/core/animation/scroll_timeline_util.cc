// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"

#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_snapshot_timeline.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"

namespace blink {

namespace scroll_timeline_util {

scoped_refptr<CompositorScrollTimeline> ToCompositorScrollTimeline(
    AnimationTimeline* timeline) {
  if (!timeline || IsA<DocumentTimeline>(timeline))
    return nullptr;

  auto* scroll_snapshot_timeline = To<ScrollSnapshotTimeline>(timeline);
  Node* scroll_source = scroll_snapshot_timeline->ResolvedSource();
  std::optional<CompositorElementId> element_id =
      GetCompositorScrollElementId(scroll_source);

  LayoutBox* box = scroll_snapshot_timeline->IsActive()
                       ? scroll_source->GetLayoutBox()
                       : nullptr;

  CompositorScrollTimeline::ScrollDirection orientation = ConvertOrientation(
      scroll_snapshot_timeline->GetAxis(), box ? box->Style() : nullptr);

  return CompositorScrollTimeline::Create(
      element_id, orientation,
      scroll_snapshot_timeline->GetResolvedScrollOffsets());
}

std::optional<CompositorElementId> GetCompositorScrollElementId(
    const Node* node) {
  if (!node || !node->GetLayoutObject() ||
      !node->GetLayoutObject()->FirstFragment().PaintProperties()) {
    return std::nullopt;
  }
  return CompositorElementIdFromUniqueObjectId(
      node->GetLayoutObject()->UniqueId(),
      CompositorElementIdNamespace::kScroll);
}

// The compositor does not know about writing modes, so we have to convert the
// web concepts of 'block' and 'inline' direction into absolute vertical or
// horizontal directions.
CompositorScrollTimeline::ScrollDirection ConvertOrientation(
    ScrollAxis axis,
    const ComputedStyle* style) {
  // Easy cases; physical is always physical.
  if (axis == ScrollAxis::kX) {
    return CompositorScrollTimeline::ScrollRight;
  }
  if (axis == ScrollAxis::kY) {
    return CompositorScrollTimeline::ScrollDown;
  }

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

  if (axis == ScrollAxis::kBlock) {
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

  DCHECK_EQ(axis, ScrollAxis::kInline);
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
