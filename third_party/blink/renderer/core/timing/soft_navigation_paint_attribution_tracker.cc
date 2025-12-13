// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_paint_attribution_tracker.h"

#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"
#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"

namespace blink {

namespace {

// When enabled, text aggregator nodes are marked as needing repaint in the
// `TextPaintTimingDetector` when the `SoftNavigationContext` associated with
// the node changes.
BASE_FEATURE(kMarkTextNodesForRepaintOnContextChange,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

SoftNavigationPaintAttributionTracker::SoftNavigationPaintAttributionTracker(
    TextPaintTimingDetector* detector)
    : text_paint_timing_detector_(detector) {
  CHECK(text_paint_timing_detector_);
}

void SoftNavigationPaintAttributionTracker::Trace(Visitor* visitor) const {
  visitor->Trace(marked_nodes_);
  visitor->Trace(text_paint_timing_detector_);
}

void SoftNavigationPaintAttributionTracker::MarkNodeAsDirectlyModified(
    Node* node,
    SoftNavigationContext* context) {
  CHECK(node);
  CHECK(context);

  // Some APIs modify text content directly, e.g. Node.nodeValue. In that case,
  // mark the parent (container) as modified to be compatible with the pre-paint
  // walk.
  if (paint_timing::IsTextType(*node)) {
    // Special case for modifying text nodes inside of a UA shadow tree, e.g.
    // changing the value attribute of <input type="button">. The parent node
    // might not have an associated layout object in that case, so we need to
    // select the shadow host (e.g. the <input>) as the container.
    if (ShadowRoot* root = node->ContainingShadowRoot();
        root && root->IsUserAgent()) {
      node = &root->host();
    } else {
      node = node->parentNode();
    }
    CHECK(node);
  }

  if (context->ContextId() != last_modification_context_id_) {
    last_modification_context_id_ = context->ContextId();
    ++current_modification_generation_id_;
  }

  // If this node is being modified again by the same context in the same
  // generation, there's no need to update anything, unless upgrading to a
  // direct modification.
  NodeState* previous_node_state = GetNodeState(node);
  if (previous_node_state) {
    if (previous_node_state->IsDirectlyModified() &&
        previous_node_state->ModificationId() ==
            current_modification_generation_id_) {
      return;
    }
  }

  marked_nodes_.Set(node, MakeGarbageCollected<NodeState>(
                              context, current_modification_generation_id_,
                              /*is_directly_modified=*/true));
  context->AddModifiedNode(node);
  if (auto* object = node->GetLayoutObject()) {
    object->MarkSoftNavigationContextChanged();
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

  NodeState* previous_node_state = GetNodeState(node);
  if (previous_node_state && previous_node_state->ModificationId() >=
                                 inherited_state->ModificationId()) {
    return;
  }
  TRACE_EVENT_INSTANT(
      TRACE_DISABLED_BY_DEFAULT("loading"),
      "SoftNavigationPaintAttributionTracker::InitPaintTrackingForNode", "node",
      node->DebugName(), "context",
      inherited_state->GetSoftNavigationContext());
  marked_nodes_.Set(node, MakeGarbageCollected<NodeState>(
                              inherited_state->GetSoftNavigationContext(),
                              inherited_state->ModificationId(),
                              /*is_directly_modified=*/false));
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
  // Continue propagating the `context_container_root` for anonymous objects.
  if (!node) {
    return PrePaintUpdateResult::kPropagateAncestorNode;
  }

  // If nothing is being propagated, there's nothing to update or track for this
  // node. Otherwise, we might need to start tracking node or update the cached
  // state if the propagated context is from a more recent modification.
  if (context_container_root) {
    auto* inherited_state = GetNodeState(context_container_root);
    CHECK(inherited_state);
    // If the `node` is something `SoftNavigationContext::AddPaintedArea()`
    // needs to know about, which is either an image or (aggregated) text.  Note
    // that this also includes nodes with background images, which may not be
    // leaf nodes -- but it's fine to store intermediate nodes in the tree whose
    // parent and descendants have the same context.
    if (paint_timing::IsTextType(*node) || paint_timing::IsImageType(object)) {
      MarkNodeForPaintTrackingIfNeeded(
          node->IsTextNode() ? text_aggregator
                             : paint_timing::ImageGeneratingNode(node),
          inherited_state);
    } else if (auto iter = marked_nodes_.find(node);
               iter != marked_nodes_.end()) {
      // Otherwise, update the cached state if the inherited context is from a
      // more recent modification.
      //
      // Note: we could overwrite the existing state, but removing it has the
      // advantage of pruning the set of redundant nodes, e.g. if a node and its
      // parent container were both modified, it's safe to remove the child
      // because we're tracking paints for the parent's whole subtree. If this
      // is removing a text aggregation node, it'll get re-added if needed when
      // the state gets propagated to its children.
      NodeState* node_state = iter->value;
      if (node_state->ModificationId() <= inherited_state->ModificationId()) {
        marked_nodes_.erase(iter);
      }
    }
  }
  // If `node` is container root that we're tracking, start propagating that to
  // descendants; otherwise keep propagating the `context_container_root`.
  if (auto* state = GetNodeState(node); state && state->IsDirectlyModified()) {
    return PrePaintUpdateResult::kPropagateCurrentNode;
  }
  return PrePaintUpdateResult::kPropagateAncestorNode;
}

void SoftNavigationPaintAttributionTracker::
    NotifyPaintTimingDetectorOnContextChanged(const LayoutObject& object) {
  if (!base::FeatureList::IsEnabled(kMarkTextNodesForRepaintOnContextChange)) {
    return;
  }
  if (paint_timing::IsImageType(object)) {
    return;
  }
  text_paint_timing_detector_->ResetPaintTrackingOnInteraction(object);
}

SoftNavigationPaintAttributionTracker::NodeState::NodeState(
    SoftNavigationContext* context,
    uint64_t modification_id,
    bool is_directly_modified)
    : context_(context),
      modification_id_(modification_id),
      is_directly_modified_(is_directly_modified) {}

void SoftNavigationPaintAttributionTracker::NodeState::Trace(
    Visitor* visitor) const {
  visitor->Trace(context_);
}

}  // namespace blink
