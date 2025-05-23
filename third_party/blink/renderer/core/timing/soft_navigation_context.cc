// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

uint64_t SoftNavigationContext::last_context_id_ = 0;

SoftNavigationContext::SoftNavigationContext(
    bool advanced_paint_attribution_enabled)
    : advanced_paint_attribution_enabled_(advanced_paint_attribution_enabled) {}

void SoftNavigationContext::AddModifiedNode(Node* node) {
  node->SetIsModifiedBySoftNavigation();

  auto add_result = modified_nodes_.insert(node);
  if (add_result.is_new_entry) {
    // If this is the first mod this animation frame, trace it.
    if (num_modified_dom_nodes_ ==
        num_modified_dom_nodes_last_animation_frame_) {
      // TODO(crbug.com/353218760): Add support for reporting every single
      // modification. Perhaps changing this to FirstModifiedNodeInFrame, and
      // then having all modifications in an even noisier trace category. Or
      // based on a chrome feature flag, for testing?
      TRACE_EVENT_INSTANT(
          TRACE_DISABLED_BY_DEFAULT("loading"),
          "SoftNavigationContext::FirstAddedModifiedNodeInAnimationFrame",
          "context", this);
    }
    ++num_modified_dom_nodes_;
  }
}

bool SoftNavigationContext::AddPaintedArea(Node* node,
                                           const gfx::RectF& rect,
                                           bool is_newest_context) {
  uint64_t painted_area = rect.size().GetArea();

  if (already_painted_modified_nodes_.Contains(node)) {
    // We are sometimes observing paints for the same node.
    // Until we fix first-contentful-paint-only observation, let's ignore these.
    repainted_area_ += painted_area;
    return false;
  }

  // For now, we only check paint after we meet other criteria, in order to
  // reduce the risk of needless tree walks during paint.
  CHECK(SatisfiesSoftNavNonPaintCriteria());

  // Iterate up the dom tree:
  for (Node* current_node = node; current_node;
       current_node = current_node->parentNode()) {
    if (current_node == known_not_related_parent_) {
      TRACE_EVENT_INSTANT(
          TRACE_DISABLED_BY_DEFAULT("loading"),
          "SoftNavigationContext::AddPaintedAreaWithEarlyExitTreeWalk",
          "context", this);
      break;
    }
    // If the current_node is known modified, it is a container root, and this
    // paint counts.
    if (modified_nodes_.Contains(current_node)) {
      already_painted_modified_nodes_.insert(node);
      // If this is the first paint this animation frame, trace it.
      if (painted_area_ == painted_area_last_animation_frame_) {
        // TODO(crbug.com/353218760): Add support for reporting every single
        // paint.
        TRACE_EVENT_INSTANT(
            TRACE_DISABLED_BY_DEFAULT("loading"),
            "SoftNavigationContext::FirstAttributablePaintInAnimationFrame",
            "context", this);
      }
      painted_area_ += painted_area;
      return true;
    }

    // For now, do not "tree walk" unless this flag is enabled.
    if (!advanced_paint_attribution_enabled_) {
      break;
    }
  }

  // This node was not part of a container root for this context.
  // Let's cache this node's parent node, so if any of this node's siblings
  // paint next, we can finish this check quicker for them.
  known_not_related_parent_ = node->parentNode();

  if (is_newest_context) {
    // We want to know how much paint the page is doing that isn't attributed.
    // We only want to do this for a single (most recent) context, in order to
    // never double count painted areas.
    unattributed_area_ += painted_area;
  }

  return false;
}

bool SoftNavigationContext::SatisfiesSoftNavNonPaintCriteria() const {
  return HasDomModification() && !url_.empty() &&
         !user_interaction_timestamp_.is_null();
}

bool SoftNavigationContext::SatisfiesSoftNavPaintCriteria(
    uint64_t required_paint_area) const {
  return painted_area_ >= required_paint_area;
}

bool SoftNavigationContext::OnPaintFinished() {
  // Reset this with each paint, since the conditions might change.
  known_not_related_parent_ = nullptr;

  auto num_modded_new_nodes =
      num_modified_dom_nodes_ - num_modified_dom_nodes_last_animation_frame_;
  auto num_gced_old_nodes = num_live_nodes_last_animation_frame_ +
                            num_modded_new_nodes - modified_nodes_.size();
  auto new_painted_area = painted_area_ - painted_area_last_animation_frame_;
  auto new_repainted_area =
      repainted_area_ - repainted_area_last_animation_frame_;

  // TODO(crbug.com/353218760): Consider reporting if any of the values change
  // if we have an extra loud tracing debug mode.
  if (num_modded_new_nodes || new_painted_area) {
    TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("loading"),
                        "SoftNavigationContext::OnPaintFinished", "context",
                        this, "numModdenNewNodes", num_modded_new_nodes,
                        "numGcedOldNodes", num_gced_old_nodes, "newPaintedArea",
                        new_painted_area, "newRepaintedArea",
                        new_repainted_area);
  }

  num_modified_dom_nodes_last_animation_frame_ = num_modified_dom_nodes_;
  num_live_nodes_last_animation_frame_ = modified_nodes_.size();
  painted_area_last_animation_frame_ = painted_area_;
  repainted_area_last_animation_frame_ = repainted_area_;

  return new_painted_area > 0;
}

void SoftNavigationContext::WriteIntoTrace(
    perfetto::TracedValue context) const {
  perfetto::TracedDictionary dict = std::move(context).WriteDictionary();

  dict.Add("softNavContextId", context_id_);
  dict.Add("interactionTimestamp", UserInteractionTimestamp());
  dict.Add("url", Url());
  dict.Add("wasEmitted", WasEmitted());

  dict.Add("domModifications", num_modified_dom_nodes_);
  dict.Add("paintedArea", painted_area_);
  dict.Add("repaintedArea", repainted_area_);
  dict.Add("unattributedPaintedArea", unattributed_area_);
}

void SoftNavigationContext::Trace(Visitor* visitor) const {
  visitor->Trace(modified_nodes_);
  visitor->Trace(already_painted_modified_nodes_);
  visitor->Trace(known_not_related_parent_);
}

}  // namespace blink
