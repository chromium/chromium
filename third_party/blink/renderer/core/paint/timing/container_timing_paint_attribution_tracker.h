// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_CONTAINER_TIMING_PAINT_ATTRIBUTION_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_CONTAINER_TIMING_PAINT_ATTRIBUTION_TRACKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class Element;
class LayoutObject;
class Node;

// Tracks which container timing root (element with `containertiming` attribute)
// is the innermost ancestor of each text aggregation node and image node.
// Populated during the pre-paint tree walk; queried at paint time in O(1).
//
// This replaces the O(depth) GetContainerRoot() DOM walk (called per painted
// element) and the GetParentContainerRoot() DOM walk (called on first paint per
// root).
//
// Only available when ContainerTimingPrepaintTraversal runtime feature is
// enabled.
//
class CORE_EXPORT ContainerTimingPaintAttributionTracker
    : public GarbageCollected<ContainerTimingPaintAttributionTracker> {
 public:
  // Mirrors SoftNavigationPaintAttributionTracker::PrePaintUpdateResult but
  // names the unit "Root" since CT only propagates container roots, not
  // arbitrary modified nodes.
  enum class PrePaintUpdateResult {
    kPropagateAncestorRoot,
    kPropagateCurrentRoot,
    kStopPropagation,
  };

  PrePaintUpdateResult UpdateOnPrePaint(const LayoutObject& object,
                                        Element* context_container_root,
                                        Node* text_aggregator);

  Element* GetContainerRootFor(Node* node) const;
  Element* GetParentContainerRootFor(Element* container_root) const;

  void Trace(Visitor*) const;

 private:
  // Marks `node` as attributed to `container_root`, or erases the existing
  // entry if `container_root` is null. Called for every walked leaf so that
  // stale entries get refreshed as the walk passes through.
  void MarkNodeForPaintTracking(Node& node, Element* container_root);

  HeapHashMap<WeakMember<Node>, WeakMember<Element>> marked_nodes_;
  HeapHashMap<WeakMember<Element>, WeakMember<Element>> container_root_parents_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_CONTAINER_TIMING_PAINT_ATTRIBUTION_TRACKER_H_
