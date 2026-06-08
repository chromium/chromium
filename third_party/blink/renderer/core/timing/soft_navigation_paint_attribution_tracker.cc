// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_paint_attribution_tracker.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"
#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"

namespace blink {

SoftNavigationPaintAttributionTracker::SoftNavigationPaintAttributionTracker(
    TextPaintTimingDetector* detector)
    : text_paint_timing_detector_(detector) {
  CHECK(text_paint_timing_detector_);
}

void SoftNavigationPaintAttributionTracker::Trace(Visitor* visitor) const {
  visitor->Trace(marked_node_state_);
  visitor->Trace(propagated_node_state_);
  visitor->Trace(text_paint_timing_detector_);
}

void SoftNavigationPaintAttributionTracker::MarkNodeAsDirectlyModified(
    Node* node,
    SoftNavigationContext* context) {
  CHECK(node);
  CHECK(context);

  // Special case for modifying text nodes inside of a UA shadow tree, e.g.
  // changing the value attribute of <input type="button">, in which case we
  // select the shadow host (e.g. the <input>) as the container.
  if (paint_timing::IsTextType(*node)) {
    if (ShadowRoot* root = node->ContainingShadowRoot();
        root && root->IsUserAgent()) {
      node = &root->host();
    }
  }

  if (context->ContextId() != last_modification_context_id_) {
    last_modification_context_id_ = context->ContextId();
    ++current_modification_generation_id_;
  }

  // If this node is being modified again by the same context in the same
  // generation, there's no need to update anything.
  NodeState* previous_node_state = GetMarkedNodeState(node);
  if (previous_node_state && previous_node_state->ModificationId() ==
                                 current_modification_generation_id_) {
    return;
  }

  // By marking the node in `marked_node_state_`, `context` will take ownership
  // of `node` and any of its descendants, which will happen during prepaint.
  marked_node_state_.Set(node,
                         MakeGarbageCollected<NodeState>(
                             context, current_modification_generation_id_));
  propagated_node_state_.erase(node);

  context->AddModifiedNode(node);
  if (auto* object = node->GetLayoutObject()) {
    // Ensure the change gets pushed down to all descendants, if modifying
    // attached DOM.
    object->MarkSoftNavigationContextChanged();
    // But only ask for repaints if the context changed. Note that the step
    // above is still needed to propagate the modification id change.
    if (!previous_node_state ||
        previous_node_state->GetSoftNavigationContext() != context) {
      NotifyPaintTimingDetectorOnContextChanged(*object);
    }
  }
}

void SoftNavigationPaintAttributionTracker::MarkNodeForPaintTrackingIfNeeded(
    Node* node,
    NodeState* inherited_state) {
  CHECK(node);
  CHECK(inherited_state);

  // For pseudo elements with background images, `node` is the parent or shadow
  // host, not the pseudo element, and it might not have an associated layout
  // object. Ignore these (PaintTimingDetector does the same).
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object) {
    return;
  }

  // If the `inherited_state` (or newer) was already propagated to `node`, don't
  // overwrite it. This happens, for example, if an aggregating node has
  // multiple text children.
  NodeState* previous_node_state = GetPropagatedNodeState(node);
  if (previous_node_state && previous_node_state->ModificationId() >=
                                 inherited_state->ModificationId()) {
    return;
  }

  // If `node` was directly modified, we don't need to duplicate the entry
  // unless `inherited_state` is newer. That can happen if the `node` was
  // modified directly (e.g. added) by a previous interaction and one of
  // `node`'s text children was directly modified by a different interaction.
  if (NodeState* marked_state = GetMarkedNodeState(node);
      marked_state &&
      marked_state->ModificationId() >= inherited_state->ModificationId()) {
    // `previous_node_state` should have been cleared when the node was
    // marked or pruned.
    CHECK(!previous_node_state, base::NotFatalUntil::M154);
    if (previous_node_state) {
      propagated_node_state_.erase(node);
    }
    return;
  }

  TRACE_EVENT_INSTANT(
      TRACE_DISABLED_BY_DEFAULT("loading"),
      "SoftNavigationPaintAttributionTracker::InitPaintTrackingForNode", "node",
      node->DebugName(), "context",
      inherited_state->GetSoftNavigationContext());
  propagated_node_state_.Set(node,
                             MakeGarbageCollected<NodeState>(
                                 inherited_state->GetSoftNavigationContext(),
                                 inherited_state->ModificationId()));
  if (!previous_node_state || previous_node_state->GetSoftNavigationContext() !=
                                  inherited_state->GetSoftNavigationContext()) {
    NotifyPaintTimingDetectorOnContextChanged(*layout_object);
  }
}

SoftNavigationPaintAttributionTracker::PrePaintUpdateResult
SoftNavigationPaintAttributionTracker::UpdateOnPrePaint(
    const LayoutObject& object,
    Node* context_container_root,
    Node* text_aggregator) {
  Node* node = object.GetNode();
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("loading"),
              "SoftNavigationPaintAttributionTracker::UpdateOnPrePaint", "node",
              node ? node->DebugName() : "(anonymous)");

  NodeState* inherited_state = context_container_root
                                   ? GetMarkedNodeState(context_container_root)
                                   : nullptr;
  // For directly modified or appended text nodes, we need to "push up" the
  // `node`'s state to the aggregator.
  if (node && paint_timing::IsTextType(*node)) {
    if (auto iter = marked_node_state_.find(node);
        iter != marked_node_state_.end()) {
      NodeState* node_state = iter->value;
      if (!inherited_state ||
          (node_state->ModificationId() > inherited_state->ModificationId())) {
        inherited_state = node_state;
      }
      // We only need to push this state up once since future modifications
      // (from above or below) will overwrite if needed.
      marked_node_state_.erase(iter);
    }
  }

  // If nothing is being propagated, there's nothing to update or track for this
  // node. Otherwise, we might need to start tracking node or update the cached
  // state if the propagated context is from a more recent modification.
  if (inherited_state) {
    // First, update the cached state if the inherited context is from a more
    // recent modification. Doing this eagerly ensures there isn't any stale
    // state for contentful nodes that can be directly marked (images).
    //
    // Note: we could overwrite the existing state, but removing it has the
    // advantage of pruning the set of redundant nodes, e.g. if a node and its
    // parent container were both modified, it's safe to remove the child
    // because we're tracking paints for the parent's whole subtree. If this
    // is removing a text aggregation node, it'll get re-added if needed when
    // the state gets propagated to its children.
    if (node) {
      MaybePruneObsoleteNodeState(node, inherited_state, marked_node_state_);
      MaybePruneObsoleteNodeState(node, inherited_state,
                                  propagated_node_state_);
    }

    // Next, set up paint tracking for contentful nodes.
    if (!node) {
      // `node` will be null (anonymous) if `object` is for a pseudo element.
      // Pseudo elements with a "content" URL are not currently handled because
      // Paint Timing doesn't handle them (related to
      // https://github.com/w3c/element-timing/issues/74).
      if (object.IsText()) {
        MarkNodeForPaintTrackingIfNeeded(text_aggregator, inherited_state);
      }
    } else if (paint_timing::IsTextType(*node) ||
               paint_timing::IsImageType(object)) {
      // If the `node` is something `SoftNavigationContext::AddPaintedArea()`
      // needs to know about, which is either an image or (aggregated) text.
      // Note that this also includes nodes with background images, which may
      // not be leaf nodes -- but it's fine to store intermediate nodes in the
      // tree whose parent and descendants have the same context.
      MarkNodeForPaintTrackingIfNeeded(
          node->IsTextNode() ? text_aggregator
                             : paint_timing::ImageGeneratingNode(node),
          inherited_state);
    }
  }

  // If `node` is container root that we're tracking, start propagating that to
  // descendants; otherwise keep propagating the `context_container_root`.
  //
  // Note: `node` may be null here (anonymous objects), in which case we
  // continue to propagate `context_container_root`.
  if (node && GetMarkedNodeState(node)) {
    return PrePaintUpdateResult::kPropagateCurrentNode;
  }
  return PrePaintUpdateResult::kPropagateAncestorNode;
}

void SoftNavigationPaintAttributionTracker::
    NotifyPaintTimingDetectorOnContextChanged(const LayoutObject& object) {
  if (paint_timing::IsImageType(object)) {
    return;
  }
  text_paint_timing_detector_->ResetPaintTrackingOnInteraction(object);
}

SoftNavigationPaintAttributionTracker::NodeState::NodeState(
    SoftNavigationContext* context,
    uint64_t modification_id)
    : context_(context), modification_id_(modification_id) {}

void SoftNavigationPaintAttributionTracker::NodeState::Trace(
    Visitor* visitor) const {
  visitor->Trace(context_);
}

// static
void SoftNavigationPaintAttributionTracker::MaybePruneObsoleteNodeState(
    Node* node,
    NodeState* inherited_state,
    NodeStateMap& node_state_map) {
  auto iter = node_state_map.find(node);
  if (iter == node_state_map.end()) {
    return;
  }
  NodeState* node_state = iter->value;
  if (node_state->ModificationId() <= inherited_state->ModificationId()) {
    node_state_map.erase(iter);
  }
}

}  // namespace blink
