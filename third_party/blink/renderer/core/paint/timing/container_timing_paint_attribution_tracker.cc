// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/container_timing_paint_attribution_tracker.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"

namespace blink {

ContainerTimingPaintAttributionTracker::PrePaintUpdateResult
ContainerTimingPaintAttributionTracker::UpdateOnPrePaint(
    const LayoutObject& object,
    Element* context_container_root,
    Node* text_aggregator) {
  Node* node = object.GetNode();

  // 1. Filter out shadow DOM, not supported.
  if (node && node->IsInShadowTree()) {
    return PrePaintUpdateResult::kPropagateAncestorRoot;
  }

  // 2. Update container timing root if changed.
  auto* element = DynamicTo<Element>(node);
  const bool has_container_timing =
      element && element->FastHasAttribute(html_names::kContainertimingAttr);
  const bool has_ignore =
      element && element->HasContainerTimingIgnoreAttribute();
  if (element) {
    if (has_container_timing) {
      container_root_parents_.Set(
          element, has_ignore ? nullptr : context_container_root);
      context_container_root = element;
    } else {
      // Not a container root: drop any stale parent mapping. This is a no-op if
      // the element was never a root, which is the common case.
      container_root_parents_.erase(element);
      if (has_ignore) {
        // containertimingignore without containertiming stops propagation:
        // this element (and its descendants, via kStopPropagation below) must
        // not be attributed to ancestor roots. Clearing the context here also
        // ensures the leaf-marking step erases any stale mapping when the
        // element itself is an image-type leaf.
        context_container_root = nullptr;
      }
    }
  }

  // 3. Mark or clear leaf node tracking. The call runs even when the context is
  // null so that a leaf previously attributed to a now-removed root gets its
  // stale entry cleared as the walk passes through.
  if (paint_timing::IsImageType(object)) {
    Node* gen_node = object.GeneratingNode();
    if (gen_node && !gen_node->IsInShadowTree()) {
      MarkNodeForPaintTracking(*gen_node, context_container_root);
    }
  } else if (!node) {
    if (object.IsText() && text_aggregator &&
        !text_aggregator->IsInShadowTree()) {
      MarkNodeForPaintTracking(*text_aggregator, context_container_root);
    }
  } else if (paint_timing::IsTextType(*node) && text_aggregator) {
    MarkNodeForPaintTracking(*text_aggregator, context_container_root);
  }

  // 4. Report back to the walk how to continue propagation.
  if (has_ignore && !has_container_timing) {
    return PrePaintUpdateResult::kStopPropagation;
  } else if (element && element == context_container_root) {
    return PrePaintUpdateResult::kPropagateCurrentRoot;
  } else {
    return PrePaintUpdateResult::kPropagateAncestorRoot;
  }
}

void ContainerTimingPaintAttributionTracker::MarkNodeForPaintTracking(
    Node& node,
    Element* container_root) {
  if (container_root) {
    marked_nodes_.Set(&node, container_root);
  } else {
    marked_nodes_.erase(&node);
  }
}

Element* ContainerTimingPaintAttributionTracker::GetContainerRootFor(
    Node* node) const {
  auto it = marked_nodes_.find(node);
  return it != marked_nodes_.end() ? it->value.Get() : nullptr;
}

Element* ContainerTimingPaintAttributionTracker::GetParentContainerRootFor(
    Element* root) const {
  auto it = container_root_parents_.find(root);
  return it != container_root_parents_.end() ? it->value.Get() : nullptr;
}

void ContainerTimingPaintAttributionTracker::Trace(Visitor* visitor) const {
  visitor->Trace(marked_nodes_);
  visitor->Trace(container_root_parents_);
}

}  // namespace blink
