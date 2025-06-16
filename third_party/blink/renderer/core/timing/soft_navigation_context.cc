// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"

namespace blink {

uint64_t SoftNavigationContext::last_context_id_ = 0;

SoftNavigationContext::SoftNavigationContext(
    LocalDOMWindow& window,
    features::SoftNavigationHeuristicsMode mode)
    : paint_attribution_mode_(mode),
      lcp_calculator_(MakeGarbageCollected<LargestContentfulPaintCalculator>(
          DOMWindowPerformance::performance(window))) {}

void SoftNavigationContext::AddModifiedNode(Node* node) {
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

bool SoftNavigationContext::IsNeededForTiming(Node* node) {
  if (!node) {
    return false;
  }
  for (Node* current_node = node; current_node;
       current_node = current_node->parentNode()) {
    if (current_node == known_not_related_parent_) {
      return false;
    }
    // If the current_node is known modified, it is a container root.
    if (modified_nodes_.Contains(current_node)) {
      return true;
    }
    // For now, do not "tree walk" when in basic mode.
    if (paint_attribution_mode_ ==
        features::SoftNavigationHeuristicsMode::kBasic) {
      break;
    }
  }
  // This node was not part of a container root for this context.
  // Let's cache this node's parent node, so if any of this node's siblings
  // paint next, we can finish this check quicker for them.
  if (Node* parent = node->parentNode()) {
    known_not_related_parent_ = parent;
  }
  return false;
}

bool SoftNavigationContext::AddPaintedArea(TextRecord* text_record) {
  Node* node = text_record->node_;
  const gfx::RectF& rect = text_record->root_visual_rect_;
  bool is_attributable = AddPaintedAreaInternal(node, rect);
  if (is_attributable) {
    if (!largest_text_ ||
        largest_text_->recorded_size < text_record->recorded_size) {
      largest_text_ = text_record;
    }
  }
  return is_attributable;
}

bool SoftNavigationContext::AddPaintedArea(ImageRecord* image_record) {
  Node* node = Node::FromDomNodeId(image_record->node_id);
  const gfx::RectF& rect = image_record->root_visual_rect;
  bool is_attributable = AddPaintedAreaInternal(node, rect);
  if (is_attributable) {
    if (!largest_image_ ||
        largest_image_->recorded_size < image_record->recorded_size) {
      largest_image_ = image_record;
    }
  }
  return is_attributable;
}

bool SoftNavigationContext::AddPaintedAreaInternal(Node* node,
                                                   const gfx::RectF& rect) {
  DCHECK(IsNeededForTiming(node));

  uint64_t painted_area = rect.size().GetArea();

  if (already_painted_modified_nodes_.Contains(node)) {
    // We are sometimes observing paints for the same node.
    // Until we fix first-contentful-paint-only observation, let's ignore these.
    repainted_area_ += painted_area;
    return false;
  }

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

bool SoftNavigationContext::SatisfiesSoftNavNonPaintCriteria() const {
  return HasDomModification() && HasUrl() &&
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

// TODO(crbug.com/419386429): This gets called after each new presentation time
// update, but this might have a range of deficiencies:
//
// 1. Candidate records might get replaced between paint and presentation.
//
// `largest_text_` and `largest_image_` are updated in `AddPaintedArea` from
// Paint stage of rendering. But `UpdateSoftLcpCandidate` is called after we
// receive frame presentation time feedback (via `PaintTimingMixin`). It is
// possible that we replace the current largest* paint record with a "pending"
// candidate, but unrelated to the presentation feedback of this
// `UpdateSoftLcpCandidate`. We should only report fully recorded paint records.
// One option is to manage a largest pending/painted recortd (like LCP
// calculator), or, just skip this next step if the candidates aren't done.
//
// 2. We might not be ready to Emit LCP candidates yet, and we might not get
// another chance later.
//
// Right now we will skip emitting LCP candidates until after soft-navigation
// entry and NavigationID are incremented.  But, this might happen after a few
// frames/paints.  Potentially unlikely given the low paint area requirement
// right now, but increasingly likely as we bump that up.
// We might want to also call `UpdateSoftLcpCandidate()` as soon as we emit
// Soft-nav entry if we already have candidates to report.  Similar to above,
// there are concerns with reporting Candidates after Paint but before
// Presentation.
void SoftNavigationContext::UpdateSoftLcpCandidate() {
  if (!WasEmitted()) {
    return;
  }
  lcp_calculator_->UpdateWebExposedLargestContentfulPaintIfNeeded(
      largest_text_, largest_image_, true);
}

void SoftNavigationContext::WriteIntoTrace(
    perfetto::TracedValue context) const {
  perfetto::TracedDictionary dict = std::move(context).WriteDictionary();

  dict.Add("softNavContextId", context_id_);
  dict.Add("interactionTimestamp", user_interaction_timestamp_);
  dict.Add("initialURL", initial_url_);
  dict.Add("mostRecentURL", most_recent_url_);
  dict.Add("wasEmitted", was_emitted_);

  dict.Add("domModifications", num_modified_dom_nodes_);
  dict.Add("paintedArea", painted_area_);
  dict.Add("repaintedArea", repainted_area_);
}

void SoftNavigationContext::Trace(Visitor* visitor) const {
  visitor->Trace(modified_nodes_);
  visitor->Trace(already_painted_modified_nodes_);
  visitor->Trace(known_not_related_parent_);
  visitor->Trace(lcp_calculator_);
  visitor->Trace(largest_text_);
  visitor->Trace(largest_image_);
}

}  // namespace blink
