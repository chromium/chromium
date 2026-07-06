// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"

#include <optional>

#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_snapshot_timeline.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
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

  return CompositorScrollTimeline::Create(
      element_id,
      ToCompositorScrollDirection(
          scroll_snapshot_timeline->GetResolvedScrollDirection()),
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

std::optional<CompositorScrollTimeline::ScrollDirection>
ToCompositorScrollDirection(std::optional<PhysicalDirection> direction) {
  if (!direction.has_value()) {
    return std::nullopt;
  }
  switch (direction.value()) {
    case PhysicalDirection::kUp:
      return CompositorScrollTimeline::ScrollUp;
    case PhysicalDirection::kRight:
      return CompositorScrollTimeline::ScrollRight;
    case PhysicalDirection::kDown:
      return CompositorScrollTimeline::ScrollDown;
    case PhysicalDirection::kLeft:
      return CompositorScrollTimeline::ScrollLeft;
  }
}

}  // namespace scroll_timeline_util

}  // namespace blink
