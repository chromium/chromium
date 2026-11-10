// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"

#include <utility>

#include "base/check_deref.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/heads_up_display_layer.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"
#include "third_party/blink/renderer/core/paint/timing/web_vitals_hud_helper.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/interaction_contentful_paint.h"
#include "third_party/blink/renderer/core/timing/interaction_effects_monitor.h"
#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace blink {

uint64_t SoftNavigationContext::last_context_id_ = 0;

SoftNavigationContext::SoftNavigationContext(
    LocalDOMWindow& window,
    PerformanceEventTiming* initial_event_timing)
    : window_(&window),
      lcp_calculator_(MakeGarbageCollected<LargestContentfulPaintCalculator>(
          DOMWindowPerformance::performance(window),
          this)),
      initial_event_timing_(initial_event_timing),
      track_(perfetto::NamedTrack::FromPointer("blink::SoftNavigation", this)) {
  CHECK(initial_event_timing_);
  CHECK(initial_event_timing_->IsInteraction());

  TRACE_EVENT_BEGIN("loading", "SoftNavigation", track_, TimeOrigin());

  TRACE_EVENT_INSTANT("loading", "SoftNavigationContextCreated", track_,
                      "context", *this);

  GetSoftNavigationHeuristics()->ForEachInteractionEffectsMonitor(
      [&](InteractionEffectsMonitor& monitor) {
        monitor.OnSoftNavigationContextCreated();
      });
}

PerformanceTimelineEntryIdInfo SoftNavigationContext::GetInteractionIdInfo()
    const {
  return initial_event_timing_->GetInteractionIdInfo().value();
}

SoftNavigationHeuristics* SoftNavigationContext::GetSoftNavigationHeuristics()
    const {
  // Before this context is Garbage-collected, it may become disposed, and
  // window_ may get cleared.
  if (HasBeenShutdown()) {
    return nullptr;
  }
  return window_->GetSoftNavigationHeuristics();
}

base::TimeTicks SoftNavigationContext::TimeOrigin() const {
  return initial_event_timing_->GetStartTime();
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
      track_, "context", this, "nodeId", node->GetDomNodeId(), "nodeDebugName",
      node->DebugName(), "domModificationsSinceLastContextPaint",
      num_modified_dom_nodes_ - previous_paint_modified_dom_nodes_);
}

void SoftNavigationContext::AddPaintedArea(
    PaintTimingRecord* record,
    uint64_t painted_area_last_animation_frame) {
  CHECK(IsRecordingLargestContentfulPaint());

  const gfx::RectF& rect = record->RootVisualRect();
  painted_area_ += rect.size().GetArea();

  Node* node = record->GetNode();
  CHECK(node);

  TRACE_EVENT_INSTANT(
      "loading", "SoftNavigationContext::AttributablePaintInAnimationFrame",
      track_, "context", this, "nodeId", node->GetDomNodeId(), "nodeDebugName",
      node->DebugName(), "rect_x", rect.x(), "rect_y", rect.y(), "rect_width",
      rect.width(), "rect_height", rect.height(),
      "paintedAreaThisAnimationFrame",
      painted_area_ - painted_area_last_animation_frame);
}

bool SoftNavigationContext::SatisfiesSoftNavNonPaintCriteria() const {
  // TODO(crbug.com/490814752): Event StartTime value seems to be missing in
  // some unittests.  It should not be missing from any real events.
  if (TimeOrigin().is_null()) {
    return false;
  }
  // These start false, and become true as we observe effects.
  if (!HasDomModification() || !HasUrl()) {
    return false;
  }
  CHECK(!UrlChangeTime().is_null());
  CHECK(!TimeOrigin().is_null());
  return true;
}

bool SoftNavigationContext::SatisfiesSoftNavPaintCriteria(
    uint64_t required_paint_area) const {
  return painted_area_ >= required_paint_area;
}

void SoftNavigationContext::OnPaintFinished(
    const HeapVector<Member<ImageRecord>>& image_records,
    const HeapVector<Member<TextRecord>>& text_records) {
  CHECK(!HasBeenShutdown());
  CHECK(IsRecordingLargestContentfulPaint());
  CHECK(!image_records.empty() || !text_records.empty());

  uint64_t new_painted_area =
      UpdatePaintedAreaOnPaintFinished(image_records, text_records);
  if (new_painted_area == 0) {
    return;
  }

  TraceOnPaintFinished(new_painted_area);

  // Notify `InteractionEffectsMonitor`s about the contentful paint.
  SoftNavigationHeuristics* heuristics = GetSoftNavigationHeuristics();
  CHECK(heuristics);
  heuristics->ForEachInteractionEffectsMonitor(
      [&](InteractionEffectsMonitor& monitor) {
        monitor.OnContentfulPaint(this, new_painted_area);
      });

  // Notify `heuristics` about the change in painted area before creating an ICP
  // entry since this might cause the navigation ID to change, if the paint
  // causes the soft navigation criteria to be met.
  heuristics->OnPaintedAreaChanged(this);
}

uint64_t SoftNavigationContext::UpdatePaintedAreaOnPaintFinished(
    const HeapVector<Member<ImageRecord>>& image_records,
    const HeapVector<Member<TextRecord>>& text_records) {
  uint64_t previous_painted_area = painted_area_;

  WebVitalsHudHelper hud_helper(
      cc::WebVitalMetricType::kInteractionContentfulPaint,
      CHECK_DEREF(window_->GetFrame()).View());
  auto process_record = [&](auto* record) {
    if (!lcp_calculator_->IsEligibleForLcp(*record)) {
      return;
    }
    hud_helper.AddWebVitalsDebugRect(*record);
    record->SetSoftNavigationContext(this);
    AddPaintedArea(record, previous_painted_area);
  };
  for (const auto& record : image_records) {
    process_record(record.Get());
  }
  for (const auto& record : text_records) {
    process_record(record.Get());
  }

  return painted_area_ - previous_painted_area;
}

void SoftNavigationContext::TraceOnPaintFinished(uint64_t new_painted_area) {
  auto modification_count =
      num_modified_dom_nodes_ - previous_paint_modified_dom_nodes_;
  previous_paint_modified_dom_nodes_ = num_modified_dom_nodes_;

  // TODO(crbug.com/353218760): Consider reporting if any of the values change
  // if we have an extra loud tracing debug mode.
  TRACE_EVENT_INSTANT("loading", "SoftNavigationContext::OnPaintFinished",
                      track_, "context", this,
                      "numModdedNodesSinceLastContextPaint", modification_count,
                      "newPaintedArea", new_painted_area);
}

void SoftNavigationContext::OnFramePresented(
    LargestContentfulPaintCalculator::LcpCandidates* candidates,
    const base::TimeTicks& presentation_timestamp,
    const DOMPaintTimingInfo& paint_timing_info,
    uint64_t navigation_id) {
  // This should not be called after we've been shut down.
  CHECK(!HasBeenShutdown());
  // TODO(crbug.com/454082773): Input should not invalidate pending presentation
  // feedback, but this can happen due to scheduling races.
  CHECK(IsRecordingLargestContentfulPaint());

  if (OptionalPaintTimingCallback callback =
          MaybeUpdateInteractionContentfulPaint(candidates)) {
    std::move(*callback).Run(presentation_timestamp, paint_timing_info,
                             navigation_id);
  }
}

OptionalPaintTimingCallback
SoftNavigationContext::MaybeUpdateInteractionContentfulPaint(
    LargestContentfulPaintCalculator::LcpCandidates* candidates) {
  // Compute new LCP candidate state and determine if we need to do anything at
  // presentation time, which we do if an ICP entry needs to be emitted or
  // metrics need to be updated. Note that the first ICP entry triggers FCP, so
  // we don't need to explicitly check for that.
  LargestContentfulPaintCalculator::PendingCandidateData candidate_data =
      lcp_calculator_->MaybeUpdateLargestContentfulPaint(candidates);
  InteractionContentfulPaint* icp_entry = nullptr;
  if (candidate_data.lcp_entry) {
    WindowPerformance* performance =
        DOMWindowPerformance::performance(*window_);
    icp_entry = MakeGarbageCollected<InteractionContentfulPaint>(
        /*start_time=*/performance->MonotonicTimeToDOMHighResTimeStamp(
            TimeOrigin()),
        candidate_data.lcp_entry, window_,
        initial_event_timing_->interactionId());
  }

  if (!candidate_data.metrics_callback && !icp_entry) {
    return std::nullopt;
  }
  return blink::BindOnce(
      &SoftNavigationContext::FlushCandidateAndMetricsOnFramePresented,
      WrapWeakPersistent(this), std::move(candidate_data.metrics_callback),
      WrapPersistent(icp_entry), std::move(candidate_data.lcp_trace_data));
}

void SoftNavigationContext::FlushCandidateAndMetricsOnFramePresented(
    OptionalPaintTimingCallback metrics_callback,
    InteractionContentfulPaint* icp_entry,
    std::unique_ptr<TracedValue> trace_data,
    const base::TimeTicks& presentation_timestamp,
    const DOMPaintTimingInfo& paint_timing_info,
    uint64_t navigation_id) {
  if (HasBeenShutdown()) {
    return;
  }

  if (metrics_callback) {
    std::move(*metrics_callback)
        .Run(presentation_timestamp, paint_timing_info, navigation_id);
  }

  // `icp_entry` can only be null here if `metrics_callback` is non-null, which
  // happens when the largest text or image was updated, but that element was
  // not larger than the current global candidate of the other type. Since that
  // requires a previous global candidate to have already been painted and
  // presented (in FIFO frame order), FCP must already be set.
  if (!icp_entry) {
    CHECK(HasFirstContentfulPaint());
    return;
  }

  icp_entry->FinalizeEntry(paint_timing_info, navigation_id);
  WindowPerformance* performance = DOMWindowPerformance::performance(*window_);
  performance->OnInteractionContentfulPaintUpdated(icp_entry);
  largest_icp_entry_ = icp_entry;

  if (trace_data) {
    TRACE_EVENT_MARK_WITH_TIMESTAMP2(
        paint_timing::kWebExposedLcpTraceCategories,
        "largestContentfulPaint::CandidateForSoftNavigation",
        presentation_timestamp, "data", std::move(trace_data), "frame",
        GetFrameIdForTracing(window_->GetFrame()));
  }

  // TODO(crbug.com/434159332): This doesn't currently match hard-FCP semantics
  // because we aren't notified about images paints until they are "sufficiently
  // loaded", which is needed for LCP/ICP.
  if (!HasFirstContentfulPaint()) {
    first_contentful_paint_ = presentation_timestamp;
    first_contentful_paint_timing_info_ = paint_timing_info;
    GetSoftNavigationHeuristics()->OnFirstContentfulPaint(this);
  }
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

const LargestContentfulPaintDetails&
SoftNavigationContext::LatestLcpDetailsForUkm() {
  return lcp_calculator_->LatestLcpDetails();
}

void SoftNavigationContext::WriteIntoTrace(
    perfetto::TracedValue context) const {
  // Ensure we don't try to trace after shutdown has been called.  If you want
  // to trace the final values-- do so right before shutdown.
  CHECK(!HasBeenShutdown());
  perfetto::TracedDictionary dict = std::move(context).WriteDictionary();

  dict.Add("softNavContextId", context_id_);
  dict.Add("performanceTimelineNavigationId", navigation_id_.web_exposed_id);

  dict.Add("URL", AttributionUrl());
  dict.Add("timeOrigin", TimeOrigin());
  dict.Add("urlChangeTime", url_change_time_);
  dict.Add("processingEnd", initial_event_timing_->GetEventTimingReportingInfo()
                                ->processing_end_time);
  dict.Add("firstContentfulPaint", FirstContentfulPaint());

  dict.Add("domModifications", num_modified_dom_nodes_);
  dict.Add("paintedArea", painted_area_);
}

void SoftNavigationContext::Trace(Visitor* visitor) const {
  visitor->Trace(lcp_calculator_);
  visitor->Trace(window_);
  visitor->Trace(largest_icp_entry_);
  visitor->Trace(initial_event_timing_);
}

void SoftNavigationContext::Shutdown() {
  TRACE_EVENT_END("loading", track_);

  lcp_calculator_ = nullptr;
  window_ = nullptr;
  largest_icp_entry_ = nullptr;
  initial_event_timing_ = nullptr;
}

void SoftNavigationContext::EmitSoftNavigation() {
  CHECK(!HasBeenShutdown());
  CHECK(!WasEmitted());
  CHECK(HasFirstContentfulPaint());
  CHECK(SatisfiesSoftNavNonPaintCriteria());
  was_emitted_ = true;

  // This trace event reports the TimeOrigin() value which we already report
  // as part of the umbrella "SoftNavigation" trace, as an instant event.
  // However, that other event reports all new *potential* soft navs, while
  // this event only reports actually *emitted* soft navs.
  // This is used by DevTools performance profiler to mark the perf timeline.
  TRACE_EVENT_INSTANT("scheduler,devtools.timeline,loading",
                      "SoftNavigationStart", track_, TimeOrigin(), "context",
                      *this, "frame",
                      GetFrameIdForTracing(window_->GetFrame()));

  // This trace event reports the when the soft nav heuristics were observerd,
  // and thus when the new navigationId was created, and when the performance
  // timeline is logically "sliced" into soft-nav sub-timelines.
  TRACE_EVENT_INSTANT("scheduler,devtools.timeline,loading",
                      "SoftNavigationEmitted", track_,
                      soft_navigation_slicing_time_, "context", *this);

  WindowPerformance* performance = DOMWindowPerformance::performance(*window_);
  CHECK(performance);
  performance->AddSoftNavigation(TimeOrigin(),
                                 first_contentful_paint_timing_info_, this);
}

void SoftNavigationContext::Dispose() {
  if (HasBeenShutdown()) {
    return;
  }
  // `heuristics` will be null if the `window_` was detached but this context
  // wasn't shut down by the associated `SoftNavigationHeuristics`, which
  // happens in some unit tests where the context isn't created by the SNH.
  SoftNavigationHeuristics* heuristics = GetSoftNavigationHeuristics();
  if (!heuristics) {
    return;
  }
  heuristics->OnContextDisposed(this);
}

void SoftNavigationContext::OnLcpMetricsForReportingChanged() {
  GetSoftNavigationHeuristics()->UpdateSoftLcpMetricsForContext(this);
}

}  // namespace blink
