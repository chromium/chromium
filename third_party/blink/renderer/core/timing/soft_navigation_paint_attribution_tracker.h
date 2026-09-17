// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_PAINT_ATTRIBUTION_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_PAINT_ATTRIBUTION_TRACKER_H_

#include <cstdint>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
class LayoutObject;
class Node;
class SoftNavigationContext;
class TextPaintTimingDetector;

// `SoftNavigationPaintAttributionTracker` helps attribute contentful paints to
// DOM nodes that were modified as part of an interaction, working together with
// `PrePaintTreeWalk` to maintain a mapping of `Node` to `SoftNavigationContext`
// for relevant nodes. It works as follows:
//
// All `LayoutObject`s have bits indicating:
//   1. whether the node's relevant `SoftNavigationContext` has changed
//      (initially true)
//   2. whether any of the node's descendants' `SoftNavigationContext` has
//      changed (initially false)
//   3. whether the node should inherit its parent's `SoftNavigationContext`
//      (initially true)
//
// After a pre-paint walk, without any interactions, bits (1) and (2) are false,
// and bit (3) is true for all to-be-painted `LayoutObject`s.
//
// When an attributable DOM modification occurs, `MarkNodeAsDirectlyModified()`
// is called for the (`Node`, `SoftNavigationContext`), and this mapping is
// recorded in `marked_node_state_`.  If the `Node` has a `LayoutObject` --
// which depends if the layout has run for the node -- bits (1) and (2) are
// updated accordingly. Note: the full tree is re-walked as needed when the DOM
// structure changes, i.e. in the case that layout hasn't run.
//
// During pre-paint, `UpdateOnPrePaint()` is called for each modified node and
// its descendants to push down the context and update internal state. The
// `SoftNavigationPaintAttributionTracker::PrePaintUpdateResult` returned by
// this method is used by the pre-paint layer to set bit (3). That bit is used
// on subsequent pre-paint walks (for non-modified nodes) to determine if the
// parent context should be used, or if this is a new root, in which case the
// this root's context will be used.
//
// Subsequent interactions: if a node is modified by more than one interaction,
// paints are attributed to the most recent modification. This is accomplished
// by updating or removing stale mappings during `UpdateOnPrePaint()`.
//
// Contentful nodes: paints are only tracked for text and image nodes, which
// allows us to avoid storing the entire descendant subtree for modified nodes.
// During pre-paint, when a contenful node is encountered, the state is
// propagated to the relevant node and stored in `propagated_node_state_`, which
// is the map used to determine the relevant context for a node during paint
// (see `GetSoftNavigationContextForNode()`). Note that since text is aggregated
// by `TextPaintTimingDetector` up to the nearest non-anonymous, non-inline
// layout object's node, this is what we store for attributable text nodes.
//
// But, we need to be careful not to over-attribute DOM modifications and
// paints. For example, <body> can be a text attribution node if a
// display:inline element is added directly to <body>, for example:
//
// <body><b>Some text</b></body>
//
// And if such an element is appended to <body>, we want to track the paints for
// the element, but not propagate the change to all descendants of <body>. This
// is handled by only propagating directly modified nodes (`marked_node_state_`)
// during the pre-paint walk and propagating state up from directly modified
// text nodes to aggregators (stored in `propagated_node_state_`).
class CORE_EXPORT SoftNavigationPaintAttributionTracker
    : public GarbageCollected<SoftNavigationPaintAttributionTracker> {
 public:
  explicit SoftNavigationPaintAttributionTracker(TextPaintTimingDetector*);

  // Initializes paint tracking for the given node, such that contentful paints
  // to it or its descendants will be associated with the given context.
  void MarkNodeAsDirectlyModified(Node*, SoftNavigationContext*);

  // Returns the `SoftNavigationContext` associated with `node` for paint
  // tracking, if any.
  SoftNavigationContext* GetSoftNavigationContextForNode(Node* node) const {
    if (NodeState* state = GetPropagatedNodeState(node)) {
      return state->GetSoftNavigationContext();
    }
    return nullptr;
  }

  // Returns true if the node is associated with the given context for paint
  // attribution, and false otherwise. The node must be a contentful node (an
  // image or text aggregation node), otherwise this returns false.
  bool IsAttributable(Node* node, SoftNavigationContext* context) const {
    if (context == nullptr) {
      return false;
    }
    return GetSoftNavigationContextForNode(node) == context;
  }

  // Returns true if the `node` is marked as a container root associated with
  // the given `context`. Note that this will return false for some directly
  // marked nodes if the node is redundant (can inherit from ancestor) or
  // obsolete (ancestor has a newer modification). Meant only for testing.
  bool IsMarkedForTesting(Node* node, SoftNavigationContext* context) const {
    if (NodeState* state = GetMarkedNodeState(node)) {
      return context == state->GetSoftNavigationContext();
    }
    return false;
  }

  // Called during the pre-paint phase to propagate the `SoftNavigationContext`
  // associated with task attributable modified DOM nodes to descendant nodes.
  // Returns a `PrePaintUpdateResult` indicating whether the current
  // `context_container_root` should continue to be propagated to this node and
  // descendants, or if `object`'s `Node` represents a new root which should be
  // be propagated to the subtree. `text_aggregator` is the ancestor to which
  // text paints will be attributed to (see `TextPaintTimingDetector`).
  enum class PrePaintUpdateResult {
    kPropagateAncestorNode,
    kPropagateCurrentNode
  };
  PrePaintUpdateResult UpdateOnPrePaint(const LayoutObject& object,
                                        Node* context_container_root,
                                        Node* text_aggregator);

  void Trace(Visitor* visitor) const;

 private:
  // State associated with nodes stored in `marked_node_state_` and
  // `propagated_node_state_`. Aside from storing the `SoftNavigationContext`,
  // this class allows us to determine the modification order between entries.
  class NodeState : public GarbageCollected<NodeState> {
   public:
    NodeState(SoftNavigationContext* context, uint64_t modification_id);

    uint64_t ModificationId() const { return modification_id_; }

    SoftNavigationContext* GetSoftNavigationContext() { return context_.Get(); }

    void Trace(Visitor* visitor) const;

   private:
    const Member<SoftNavigationContext> context_;
    const uint64_t modification_id_;
  };

  // TODO(crbug.com/423670827): `NodeState` currently keeps the associated
  // `SoftNavigationContext` alive, so the context's lifetime depends on the
  // lifetime of the nodes it modified. We may want to consider making this
  // eligible for GC earlier, but need to figure out how attribution should be
  // handled if there are ancestors with an older context.
  using NodeStateMap = HeapHashMap<WeakMember<Node>, Member<NodeState>>;

  NodeState* GetMarkedNodeState(Node* node) const {
    auto iter = marked_node_state_.find(node);
    return iter == marked_node_state_.end() ? nullptr : iter->value;
  }

  NodeState* GetPropagatedNodeState(Node* node) const {
    auto iter = propagated_node_state_.find(node);
    return iter == propagated_node_state_.end() ? nullptr : iter->value;
  }

  // Tracks `node` in `propagated_node_state_` if the `inherited_state` is from
  // a more recent modification, or if `node` isn't being tracked. Called for
  // "contentful nodes" (images and text aggregation nodes).
  void MarkNodeForPaintTrackingIfNeeded(Node* node, NodeState* inherited_state);

  // Inform the relevant paint timing detector that we need paint tracking for
  // the object -- regardless of whether its been previously painted -- because
  // its `SoftNavigationContext` changed.
  void NotifyPaintTimingDetectorOnContextChanged(const LayoutObject&);

  // IDs used for determining the modification order of `NodeState` objects.  We
  // use a "modification generation" scheme, incrementing the
  // `current_modification_generation_id_` when `MarkNodeAsDirectlyModified()`
  // is called with a different context from the last time. This enables
  // grouping related DOM modifications, which enables pruning redundant
  // `marked_node_state_`.
  uint64_t current_modification_generation_id_ = 0;
  uint64_t last_modification_context_id_ = 0;

  // Stores `NodeState` for nodes directly modified by an interaction. Redundant
  // nodes (e.g. a node whose parent is also marked) and obsolete nodes
  // (descendants of nodes with newer modifications) are removed during
  // pre-paint.
  NodeStateMap marked_node_state_;

  // Stores `NodeState` for text aggregation and image elements whose state was
  // propagated from a marked container they belong to. For example, if a <div>
  // contains an image and the <div> was directly modified (i.e. in
  // `marked_node_state_`), then <div>'s `NodeState` is propagated to image and
  // stored in this map.
  //
  // Note that while state is typically pushed down from marked nodes (parent
  // containers) to children, it can also be pushed up from individually
  // modified or appended text nodes. Separating `marked_node_state_` from
  // `propagated_node_state_` ensures state is not pushed down to a text node's
  // siblings in that case (unless the parent was also marked).
  NodeStateMap propagated_node_state_;

  Member<TextPaintTimingDetector> text_paint_timing_detector_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SOFT_NAVIGATION_PAINT_ATTRIBUTION_TRACKER_H_
