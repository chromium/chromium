// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/interaction_effects_monitor.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"

namespace blink {

uint64_t SoftNavigationContext::last_context_id_ = 0;

SoftNavigationContext::SoftNavigationContext(LocalDOMWindow& window)
    : window_(&window),
      lcp_calculator_(MakeGarbageCollected<LargestContentfulPaintCalculator>(
          DOMWindowPerformance::performance(window),
          this)) {
  window_->GetSoftNavigationHeuristics()->ForEachInteractionEffectsMonitor(
      [&](InteractionEffectsMonitor& monitor) {
        monitor.OnSoftNavigationContextCreated();
      });
}

void SoftNavigationContext::AddModifiedNode(Node* node) {
  ++num_modified_dom_nodes_;
  TRACE_EVENT_INSTANT(
      "loading", "SoftNavigationContext::AddedModifiedNodeInAnimationFrame",
      perfetto::Track::FromPointer(this), "context", this, "nodeId",
      node->GetDomNodeId(), "nodeDebugName", node->DebugName(),
      "domModificationsThisAnimationFrame",
      num_modified_dom_nodes_ - num_modified_dom_nodes_last_animation_frame_);
}

bool SoftNavigationContext::AddPaintedArea(PaintTimingRecord* record) {
  // Stop recording paints once we have next input/scroll.
  if (!first_input_or_scroll_time_.is_null()) {
    return false;
  }

  const gfx::RectF& rect = record->RootVisualRect();
  uint64_t painted_area = rect.size().GetArea();

  Node* node = record->GetNode();
  // TODO(crbug.com/441914208): `node` can be null here, which is unexpected.
  // Change this back to a CHECK when the root cause is understood and fixed.
  if (!node) {
    return false;
  }

  painted_area_ += painted_area;
  TRACE_EVENT_INSTANT(
      "loading", "SoftNavigationContext::AttributablePaintInAnimationFrame",
      perfetto::Track::FromPointer(this), "context", this, "nodeId",
      node->GetDomNodeId(), "nodeDebugName", node->DebugName(), "rect_x",
      rect.x(), "rect_y", rect.y(), "rect_width", rect.width(), "rect_height",
      rect.height(), "paintedAreaThisAnimationFrame",
      painted_area_ - painted_area_last_animation_frame_);

  // TODO(crbug.com/434159332): This doesn't currently match hard-FCP semantics
  // because we aren't notified about images paints until they are "sufficiently
  // loaded", which is needed for LCP/ICP.
  if (!first_image_or_text_) {
    first_image_or_text_ = record;
  }

  if (record->IsImageRecord()) {
    if (!largest_image_ ||
        largest_image_->RecordedSize() < record->RecordedSize()) {
      largest_image_ = To<ImageRecord>(record);
    }
  } else {
    CHECK(record->IsTextRecord());
    if (!largest_text_ ||
        largest_text_->RecordedSize() < record->RecordedSize()) {
      largest_text_ = To<TextRecord>(record);
    }
  }

  return true;
}

bool SoftNavigationContext::SatisfiesSoftNavNonPaintCriteria() const {
  return HasDomModification() && HasUrl() && !time_origin_.is_null();
}

bool SoftNavigationContext::SatisfiesSoftNavPaintCriteria(
    uint64_t required_paint_area) const {
  return painted_area_ >= required_paint_area;
}

bool SoftNavigationContext::OnPaintFinished() {
  auto num_modded_new_nodes =
      num_modified_dom_nodes_ - num_modified_dom_nodes_last_animation_frame_;
  auto new_painted_area = painted_area_ - painted_area_last_animation_frame_;

  // TODO(crbug.com/353218760): Consider reporting if any of the values change
  // if we have an extra loud tracing debug mode.
  if (num_modded_new_nodes || new_painted_area) {
    TRACE_EVENT_INSTANT("loading", "SoftNavigationContext::OnPaintFinished",
                        perfetto::Track::FromPointer(this), "context", this,
                        "numModdedNewNodes", num_modded_new_nodes,
                        "newPaintedArea", new_painted_area);
  }

  if (new_painted_area > 0) {
    window_->GetSoftNavigationHeuristics()->ForEachInteractionEffectsMonitor(
        [&](InteractionEffectsMonitor& monitor) {
          monitor.OnContentfulPaint(this, new_painted_area);
        });
  }

  num_modified_dom_nodes_last_animation_frame_ = num_modified_dom_nodes_;
  painted_area_last_animation_frame_ = painted_area_;

  return new_painted_area > 0;
}

void SoftNavigationContext::OnInputOrScroll() {
  if (!first_input_or_scroll_time_.is_null()) {
    return;
  }
  TRACE_EVENT_INSTANT("loading", "SoftNavigationContext::OnInputOrScroll",
                      "painted_area", painted_area_);
  // Between interaction and first painted area, we allow other inputs or
  // scrolling to happen.  Once we observe the first paint, we have to constrain
  // to that initial viewport, or else the viewport area and set of candidates
  // gets messy.
  if (!painted_area_) {
    return;
  }
  first_input_or_scroll_time_ = base::TimeTicks::Now();
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
void SoftNavigationContext::UpdateWebExposedLargestContentfulPaintIfNeeded() {
  lcp_calculator_->UpdateWebExposedLargestContentfulPaintIfNeeded(
      largest_text_, largest_image_);
}

bool SoftNavigationContext::TryUpdateLcpCandidate() {
  // After we are ready to start measuring LCP (after the soft nav entry was
  // emitted) and before we want to stop (input or scroll), we update LCP
  // candidate.
  if (!was_emitted_ || !first_input_or_scroll_time_.is_null()) {
    return false;
  }

  bool latest_lcp_details_for_ukm_changed = false;
  // TODO(crbug.com/425989954): Guard on paint_time, because although this
  // TryUpdateLcpCandidate gets called after presentation feedback, it might not
  // be the right presentation time for this specific text/image record.
  if (largest_text_ && largest_text_->HasPaintTime()) {
    latest_lcp_details_for_ukm_changed =
        latest_lcp_details_for_ukm_changed ||
        lcp_calculator_->NotifyMetricsIfLargestTextPaintChanged(
            *largest_text_.Get());
  }
  if (largest_image_ && largest_image_->HasPaintTime()) {
    latest_lcp_details_for_ukm_changed =
        latest_lcp_details_for_ukm_changed ||
        lcp_calculator_->NotifyMetricsIfLargestImagePaintChanged(
            *largest_image_.Get());
  }
  return latest_lcp_details_for_ukm_changed;
}

const LargestContentfulPaintDetails&
SoftNavigationContext::LatestLcpDetailsForUkm() {
  return lcp_calculator_->LatestLcpDetails();
}

void SoftNavigationContext::WriteIntoTrace(
    perfetto::TracedValue context) const {
  perfetto::TracedDictionary dict = std::move(context).WriteDictionary();

  dict.Add("softNavContextId", context_id_);
  dict.Add("performanceTimelineNavigationId", navigation_id_);

  dict.Add("URL", AttributionUrl());
  dict.Add("timeOrigin", time_origin_);
  dict.Add("firstContentfulPaint", FirstContentfulPaint());

  dict.Add("domModifications", num_modified_dom_nodes_);
  dict.Add("paintedArea", painted_area_);
}

void SoftNavigationContext::Trace(Visitor* visitor) const {
  visitor->Trace(lcp_calculator_);
  visitor->Trace(largest_text_);
  visitor->Trace(largest_image_);
  visitor->Trace(first_image_or_text_);
  visitor->Trace(window_);
}

void SoftNavigationContext::Shutdown() {
  lcp_calculator_ = nullptr;
  largest_text_ = nullptr;
  largest_image_ = nullptr;
  first_image_or_text_ = nullptr;
  window_ = nullptr;
}

void SoftNavigationContext::EmitLcpPerformanceEntry(
    const DOMPaintTimingInfo& paint_timing_info,
    uint64_t paint_size,
    base::TimeTicks load_time,
    const AtomicString& id,
    const String& url,
    Element* element) {
  // TODO(crbug.com/454082771): We currently only expect this to be called once
  // the soft nav entry has been emitted, but it's possible for some of the info
  // to be lost if the node is removed before all the conditions are met.
  // Instead, we should buffer the most recent candidate and emit it along with
  // the soft nav entry, which avoids hanging onto the PaintTimingRecord
  // indefinitely.
  CHECK(WasEmitted());
  // This should not be called after we've been shut down.
  CHECK(window_);
  DOMWindowPerformance::performance(*window_)
      ->OnInteractionContentfulPaintUpdated(paint_timing_info, paint_size,
                                            load_time, id, url, element,
                                            NavigationId());
}

}  // namespace blink
