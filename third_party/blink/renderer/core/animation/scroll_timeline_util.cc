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

  PhysicalToLogical<CompositorScrollTimeline::ScrollDirection> converter(
      style ? style->GetWritingDirection()
            : WritingDirectionMode(WritingMode::kHorizontalTb,
                                   TextDirection::kLtr),
      CompositorScrollTimeline::ScrollUp, CompositorScrollTimeline::ScrollRight,
      CompositorScrollTimeline::ScrollDown,
      CompositorScrollTimeline::ScrollLeft);
  if (axis == ScrollAxis::kBlock) {
    return converter.BlockEnd();
  }
  DCHECK_EQ(axis, ScrollAxis::kInline);
  return converter.InlineEnd();
}

}  // namespace scroll_timeline_util

}  // namespace blink
