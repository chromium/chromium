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
  // That process handles updating the `propagated_node_state_` and resetting
  // paint tracking if needed.
  marked_node_state_.Set(node,
                         MakeGarbageCollected<NodeState>(
                             context, current_modification_generation_id_));
  context->AddModifiedNode(node);
  // Ensure the change gets pushed down to all descendants, if modifying
  // laid-out DOM.
  if (auto* object = node->GetLayoutObject()) {
    object->MarkSoftNavigationContextChanged();
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

  TRACE_EVENT_INSTANT(
      TRACE_DISABLED_BY_DEFAULT("loading"),
      "SoftNavigationPaintAttributionTracker::InitPaintTrackingForNode", "node",
      node->DebugName(), "context",
      inherited_state->GetSoftNavigationContext());
  propagated_node_state_.Set(node, inherited_state);

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

  // By default, continue to inherit the context being propagated. This gets
  // overridden below if a new container root is detected.
  auto result = PrePaintUpdateResult::kPropagateAncestorNode;

  // First, figure out what should be propagated, i.e. if `node` is a new
  // container root, if it should inherit the `inherited_state`, or if
  // `NodeState` for a text node needs to be pushed up.
  if (node) {
    if (auto iter = marked_node_state_.find(node);
        iter != marked_node_state_.end()) {
      NodeState* node_state = iter->value;
      if (!inherited_state ||
          node_state->ModificationId() > inherited_state->ModificationId()) {
        // `node` is a new container root. For text nodes, this pushes up the
        // `node_state` to the aggregator; for other nodes, it gets propagated
        // downwards.
        inherited_state = node_state;
        // We only need to push this state up once for text since future
        // modifications (from above or below) will overwrite if needed. And
        // since text nodes are leaf nodes, propagation will stop after this so
        // there is nothing to push down to.
        if (paint_timing::IsTextType(*node)) {
          marked_node_state_.erase(iter);
        }
        result = PrePaintUpdateResult::kPropagateCurrentNode;
      } else {
        // `node_state` is redundant or obsolete. Remove it and continue
        // propagating `inherited_state`.
        //
        // Note: we could overwrite the existing state, but removing it has the
        // advantage of pruning the set of redundant nodes, e.g. if a node and
        // its parent container were both modified, it's safe to remove the
        // child because we're tracking paints for the parent's whole subtree.
        // If this is removing a text aggregation node or image, it'll get
        // mapped in the `propagated_node_state_` if needed.
        marked_node_state_.erase(iter);
      }
    }
  }

  // If nothing is being propagated, there's nothing to update or track for this
  // node. Otherwise, we might need to start tracking `node` or update the
  // cached state if the propagated context is from a more recent modification.
  if (inherited_state) {
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
          node->IsTextNode() ? text_aggregator : object.GeneratingNode(),
          inherited_state);
    }
  }

  return result;
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

}  // namespace blink
