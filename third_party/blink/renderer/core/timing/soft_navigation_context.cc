// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"

#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/interaction_contentful_paint.h"
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

base::TimeTicks SoftNavigationContext::TimeOrigin() const {
  if (processing_end_.is_null()) {
    return url_change_time_;
  }
  if (url_change_time_.is_null()) {
    return processing_end_;
  }
  return std::min(url_change_time_, processing_end_);
}

void SoftNavigationContext::AddUrl(
    const String& url,
    V8NavigationType::Enum navigation_type,
    base::UnguessableToken same_document_metrics_token) {
  // The navigation layer should never pass an empty URL.
  CHECK(!url.empty());

  // An interaction can lead to multiple URL changes, e.g. because of
  // client-side redirects. Subsequent URL changes are no-ops.
  if (!initial_url_.empty()) {
    return;
  }
  initial_url_ = url;
  navigation_type_ = navigation_type;
  same_document_metrics_token_ = same_document_metrics_token;
  url_change_time_ = base::TimeTicks::Now();
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
    lcp_calculator_->MaybeUpdateLargestPaintedImage(To<ImageRecord>(record));
  } else {
    CHECK(record->IsTextRecord());
    lcp_calculator_->MaybeUpdateLargestText(To<TextRecord>(record));
  }

  return true;
}

bool SoftNavigationContext::SatisfiesSoftNavNonPaintCriteria() const {
  if (HasDomModification() && HasUrl() && !ProcessingEnd().is_null()) {
    CHECK(!UrlChangeTime().is_null());  // Implied by HasUrl()
    // Implied by !UrlChangeTime().is_null() and !ProcessingEnd().is_null()
    CHECK(!TimeOrigin().is_null());
    return true;
  }
  return false;
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
// 2. We might not be ready to emit LCP candidates yet.
//
// Right now we skip emitting LCP candidates until after the `navigation_id_` is
// set and the soft navigation entry is emitted, which might happen after a few
// frames/paints. We do buffer the most recent candidate and emit that if and
// when the soft navigation entry is emitted, but we might want to consider
// buffering and emitting more candidates.
void SoftNavigationContext::TryUpdateLcpCandidate() {
  // TODO(crbug.com/454082773): Input should not invalidate pending presentation
  // feedback, but this can happen due to scheduling races.
  if (!IsRecordingLargestContentfulPaint()) {
    return;
  }
  lcp_calculator_->MaybeFlushCandidates();
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
  dict.Add("timeOrigin", TimeOrigin());
  dict.Add("urlChangeTime", url_change_time_);
  dict.Add("processingEnd", processing_end_);
  dict.Add("firstContentfulPaint", FirstContentfulPaint());

  dict.Add("domModifications", num_modified_dom_nodes_);
  dict.Add("paintedArea", painted_area_);
}

void SoftNavigationContext::Trace(Visitor* visitor) const {
  visitor->Trace(lcp_calculator_);
  visitor->Trace(first_image_or_text_);
  visitor->Trace(window_);
}

void SoftNavigationContext::Shutdown() {
  lcp_calculator_ = nullptr;
  first_image_or_text_ = nullptr;
  window_ = nullptr;
}

void SoftNavigationContext::EmitSoftNavigation() {
  CHECK(!WasEmitted());
  CHECK(HasFirstContentfulPaint());
  was_emitted_ = true;

  if (base::FeatureList::IsEnabled(kSoftNavigationTraceEvents)) {
    TRACE_EVENT_INSTANT(
        "scheduler,devtools.timeline,loading", "SoftNavigationStart",
        perfetto::Track::FromPointer(this), TimeOrigin(), "context", *this,
        "frame", GetFrameIdForTracing(window_->GetFrame()));
  }

  if (!RuntimeEnabledFeatures::SoftNavigationHeuristicsEnabled(window_)) {
    return;
  }

  WindowPerformance* performance = DOMWindowPerformance::performance(*window_);
  CHECK(performance);
  performance->AddSoftNavigationEntry(
      AtomicString(AttributionUrl()), TimeOrigin(),
      FirstContentfulPaintTimingInfo(), NavigationId(), NavigationType());
}

void SoftNavigationContext::Dispose() {
  // `window_` will be null if this context was already shut down.
  if (!window_) {
    return;
  }
  // `heuristics` will be null if the `window_` was detached but this context
  // wasn't shut down by the associated `SoftNavigationHeuristics`, which
  // happens in some unit tests where the context isn't created by the SNH.
  SoftNavigationHeuristics* heuristics = window_->GetSoftNavigationHeuristics();
  if (!heuristics) {
    return;
  }
  heuristics->OnContextDisposed(this);
}

void SoftNavigationContext::EmitLcpPerformanceEntry(
    const DOMPaintTimingInfo& paint_timing_info,
    uint64_t paint_size,
    base::TimeTicks load_time,
    const AtomicString& id,
    const String& url,
    Element* element) {
  if (!RuntimeEnabledFeatures::SoftNavigationHeuristicsEnabled(window_)) {
    return;
  }

  // This should not be called after we've been shut down.
  CHECK(window_);
  WindowPerformance* performance = DOMWindowPerformance::performance(*window_);
  auto* entry = MakeGarbageCollected<InteractionContentfulPaint>(
      /*start_time=*/paint_timing_info.presentation_time,
      /*render_time=*/paint_timing_info.presentation_time, paint_size,
      performance->MonotonicTimeToDOMHighResTimeStamp(load_time), id, url,
      element, window_, navigation_id_);
  entry->SetPaintTimingInfo(paint_timing_info);
  performance->OnInteractionContentfulPaintUpdated(entry);
}

void SoftNavigationContext::OnLcpMetricsForReportingChanged() {
  window_->GetSoftNavigationHeuristics()->UpdateSoftLcpMetricsForContext(this);
}

}  // namespace blink
