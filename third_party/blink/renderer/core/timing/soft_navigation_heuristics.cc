// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"

#include <cstdint>
#include <iterator>
#include <utility>

#include "base/containers/adapters.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_type.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_type_util.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/interaction_effects_monitor.h"
#include "third_party/blink/renderer/core/timing/performance_timing_for_reporting.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_paint_attribution_tracker.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

namespace blink {

namespace {

const char kPageLoadInternalSoftNavigationOutcome[] =
    "PageLoad.Internal.SoftNavigationOutcome";

const char kPageLoadInternalSoftNavigationEmittedTotalPaintArea[] =
    "PageLoad.Internal.SoftNavigation.Emitted.TotalPaintArea";
const char kPageLoadInternalSoftNavigationEmittedTotalPaintAreaPoints[] =
    "PageLoad.Internal.SoftNavigation.Emitted.TotalPaintAreaPoints";

const char kPageLoadInternalSoftNavigationNotEmittedUrlEmptyTotalPaintArea[] =
    "PageLoad.Internal.SoftNavigation.NotEmittedUrlEmpty.TotalPaintArea";
const char
    kPageLoadInternalSoftNavigationNotEmittedUrlEmptyTotalPaintAreaPoints[] =
        "PageLoad.Internal.SoftNavigation.NotEmittedUrlEmpty."
        "TotalPaintAreaPoints";
const char
    kPageLoadInternalSoftNavigationNotEmittedInsufficientPaintTotalPaintArea[] =
        "PageLoad.Internal.SoftNavigation.NotEmittedInsufficientPaint."
        "TotalPaintArea";
const char
    kPageLoadInternalSoftNavigationNotEmittedInsufficientPaintTotalPaintAreaPercentage
        [] = "PageLoad.Internal.SoftNavigation.NotEmittedInsufficientPaint."
             "TotalPaintAreaPoints";
// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SoftNavigationOutcome" in tools/metrics/histograms/enums.xml. Note also that
// these form a bitmask; future conditions should continue this pattern.
// LINT.IfChange
enum SoftNavigationOutcome {
  kSoftNavigationDetected = 0,

  kNoSoftNavContextDuringUrlChange = 1 << 0,
  kInsufficientPaints = 1 << 1,
  kNoDomModification = 1 << 2,
  kNoSoftNavContextDuringUrlChangeButMergingIntoPreviousContext = 1 << 3,

  // For now, this next value is equivalent to kNoDomModification, because we
  // cannot have paints without a dom mod.
  // However, kNoDomModification might evolve into something more "semantic",
  // such that you could have paints without a dom mod.
  kNoPaintOrDomModification = kInsufficientPaints | kNoDomModification,

  kMaxValue = kNoSoftNavContextDuringUrlChangeButMergingIntoPreviousContext,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:SoftNavigationOutcome)

void OnSoftNavigationContextWasExhausted(const SoftNavigationContext& context,
                                         uint64_t viewport_area,
                                         uint64_t required_paint_area) {
  TRACE_EVENT_INSTANT(
      "loading", "SoftNavigationHeuristics::SoftNavigationContextWasExhausted",
      perfetto::Track::FromPointer(&context), "context", context);

  // Don't bother to log if the URL was never set.  That means it was just a
  // normal interaction.
  if (!context.HasUrl()) {
    uint64_t total_paint_area = context.PaintedArea();
    base::UmaHistogramCounts1M(
        kPageLoadInternalSoftNavigationNotEmittedUrlEmptyTotalPaintArea,
        total_paint_area);

    // viewport_area is guaranteed to be >= 1.
    uint64_t points_val = (total_paint_area * 10000ULL) / viewport_area;
    base::UmaHistogramCounts100000(
        kPageLoadInternalSoftNavigationNotEmittedUrlEmptyTotalPaintAreaPoints,
        base::saturated_cast<int>(points_val));
    return;
  }

  // TODO(crbug.com/351826232): Consider differentiating contexts that were
  // cleaned up before page was unloaded vs cleaned up because of page unload.

  if (context.HasNavigationId()) {
    // We already report this outcome eagerly, as part of
    // `ReportSoftNavigationToMetrics`, so don't report again here.
    // However, we can report the final paint area metrics here.
    uint64_t total_paint_area = context.PaintedArea();
    base::UmaHistogramCounts1M(
        kPageLoadInternalSoftNavigationEmittedTotalPaintArea, total_paint_area);

    // viewport_area is guaranteed to be >= 1.
    uint64_t points_val = (total_paint_area * 10000ULL) / viewport_area;
    base::UmaHistogramCounts100000(
        kPageLoadInternalSoftNavigationEmittedTotalPaintAreaPoints,
        base::saturated_cast<int>(points_val));
  } else if (!context.HasDomModification()) {
    base::UmaHistogramEnumeration(kPageLoadInternalSoftNavigationOutcome,
                                  SoftNavigationOutcome::kNoDomModification);
  } else if (!context.SatisfiesSoftNavPaintCriteria(required_paint_area)) {
    base::UmaHistogramEnumeration(kPageLoadInternalSoftNavigationOutcome,
                                  SoftNavigationOutcome::kInsufficientPaints);
    uint64_t total_paint_area = context.PaintedArea();
    base::UmaHistogramCounts1M(
        kPageLoadInternalSoftNavigationNotEmittedInsufficientPaintTotalPaintArea,
        total_paint_area);

    // viewport_area is guaranteed to be >= 1.
    uint64_t points_val = (total_paint_area * 10000ULL) / viewport_area;
    base::UmaHistogramCounts100000(
        kPageLoadInternalSoftNavigationNotEmittedInsufficientPaintTotalPaintAreaPercentage,
        base::saturated_cast<int>(points_val));
  }
}

SoftNavigationHeuristics* GetHeuristicsForNodeIfShouldTrack(const Node& node) {
  // This handles both disconnected nodes and detached frames.
  if (!node.InActiveDocument()) {
    return nullptr;
  }
  // The window cannot be null unless the document has been shut down, which is
  // not true for active documents.
  LocalDOMWindow* window = node.GetDocument().domWindow();
  CHECK(window);
  return window->GetSoftNavigationHeuristics();
}

using LcpCandidates = LargestContentfulPaintCalculator::LcpCandidates;
using ContextToCandidatesMap =
    HeapHashMap<Member<SoftNavigationContext>, Member<LcpCandidates>>;

template <IsDerivedFromPaintTimingRecord T>
void GroupLcpCandidatesByContext(const HeapVector<Member<T>>& records,
                                 ContextToCandidatesMap& context_map) {
  for (const auto& record : records) {
    SoftNavigationContext* context = record->GetSoftNavigationContext();
    if (!context || !context->IsRecordingLargestContentfulPaint()) {
      continue;
    }
    LcpCandidates* candidates = nullptr;
    if (auto iter = context_map.find(context); iter != context_map.end()) {
      candidates = iter->value.Get();
    } else {
      candidates = MakeGarbageCollected<LcpCandidates>();
      context_map.insert(context, candidates);
    }
    candidates->MaybeUpdateCandidate(record);
  }
}

}  // namespace

SoftNavigationHeuristics::SoftNavigationHeuristics(LocalDOMWindow* window)
    : window_(window),
      task_attribution_tracker_(
          scheduler::TaskAttributionTracker::From(window->GetIsolate())) {
  LocalFrame* frame = window->GetFrame();
  CHECK(frame && frame->View());
  TextPaintTimingDetector* detector =
      &frame->View()->GetPaintTimingDetector().GetTextPaintTimingDetector();
  paint_attribution_tracker_ =
      MakeGarbageCollected<SoftNavigationPaintAttributionTracker>(detector);
}

SoftNavigationHeuristics* SoftNavigationHeuristics::CreateIfNeeded(
    LocalDOMWindow* window) {
  if (!base::FeatureList::IsEnabled(features::kSoftNavigationDetection)) {
    return nullptr;
  }
  // We expect the window to be valid and the frame to be attached.
  CHECK(window && window->GetFrame() && window->GetFrame()->GetPage());

  // Soft navigations in iframes are not supported.
  if (!window->GetFrame()->IsOutermostMainFrame()) {
    return nullptr;
  }
  // Filter out non-ordinary pages, e.g. devtools overlays and internal pages
  // used for SVG image rendering. Soft navigations are only intended to be
  // measured on web developer-authored pages.
  if (!window->GetFrame()->GetPage()->IsOrdinary()) {
    return nullptr;
  }
  if (Document* document = window->document()) {
    // Don't measure soft navigations in devtools.
    if (document->Url().ProtocolIs("devtools")) {
      return nullptr;
    }
  }
  return MakeGarbageCollected<SoftNavigationHeuristics>(window);
}

void SoftNavigationHeuristics::Shutdown() {
  task_attribution_tracker_ = nullptr;

  const auto viewport_area = CalculateViewportArea();
  const auto required_paint_area = CalculateRequiredPaintArea();
  for (const auto& context : interaction_id_to_context_.Values()) {
    OnSoftNavigationContextWasExhausted(*context, viewport_area,
                                        required_paint_area);
    context->Shutdown();
  }

  for (const auto& monitor : interaction_effects_monitors_) {
    monitor->Shutdown();
  }
  interaction_effects_monitors_.clear();

  interaction_id_to_context_.clear();
}

SoftNavigationContext*
SoftNavigationHeuristics::GetSoftNavigationContextForInteractionId(
    PerformanceTimelineEntryIdInfo interaction_id) const {
  if (interaction_id == PerformanceTimelineEntryIdInfo::kNone) {
    return nullptr;
  }
  auto it = interaction_id_to_context_.find(interaction_id.id);
  if (it != interaction_id_to_context_.end()) {
    return it->value.Get();
  }
  return nullptr;
}

SoftNavigationContext*
SoftNavigationHeuristics::GetSoftNavigationContextForCurrentTask() const {
  if (interaction_id_to_context_.empty()) {
    return nullptr;
  }
  // The `task_attribution_tracker_` must exist if `interaction_id_to_context_`
  // is non-empty. `task_state` can have null `context` in tests.
  CHECK(task_attribution_tracker_);
  if (auto* task_state = task_attribution_tracker_->CurrentTaskState()) {
    SoftNavigationContext* context = task_state->GetSoftNavigationContext();
    // Even when we have a context, we need to confirm if this SNH instance
    // is tracking it. If the context comes from a task that crossed from
    // another window, we might have a different SNH instance. This seems to
    // fail with datetime/calendar modals, for example.
    // TODO(crbug.com/40871933): We don't care to support datetime modals, but
    // this behaviour might be similar for iframes, and might be worth
    // supporting.
    if (context && context->GetSoftNavigationHeuristics() == this) {
      return context;
    }
  }
  return nullptr;
}

SoftNavigationContext*
SoftNavigationHeuristics::GetRelevantContextForNavigation(
    std::optional<PerformanceTimelineEntryIdInfo> interaction_id) const {
  SoftNavigationContext* context_for_task =
      GetSoftNavigationContextForCurrentTask();

  SoftNavigationContext* context_for_id = nullptr;
  if (interaction_id.has_value() &&
      interaction_id.value() != PerformanceTimelineEntryIdInfo::kNone) {
    context_for_id =
        GetSoftNavigationContextForInteractionId(interaction_id.value());
  }

  CHECK(!context_for_task || !context_for_id ||
            context_for_task == context_for_id,
        base::NotFatalUntil::M153);

  return context_for_id ? context_for_id : context_for_task;
}

void SoftNavigationHeuristics::SameDocumentNavigationCommitted(
    const KURL& old_url,
    const KURL& new_url,
    WebFrameLoadType load_type,
    base::UnguessableToken same_document_metrics_token,
    PerformanceTimelineEntryIdInfo interaction_id) {
  if (load_type == WebFrameLoadType::kReplaceCurrentItem &&
      !RuntimeEnabledFeatures::
          SoftNavigationDetectionIncludeReplaceStateEnabled()) {
    return;
  }

  if (new_url == old_url) {
    return;
  }

  SoftNavigationContext* context =
      GetRelevantContextForNavigation(interaction_id);

  String new_url_string = new_url.GetString();
  if (!context && !context_for_current_url_) {
    // If we don't have a context for this task, and we haven't had a context
    // for a recent URL change, then this URL change is not a soft-navigation.
    TRACE_EVENT_INSTANT("loading",
                        "SoftNavigationHeuristics::"
                        "SameDocumentNavigationCommittedWithoutContext",
                        perfetto::Track::FromPointer(this), "url",
                        new_url_string);
    base::UmaHistogramEnumeration(
        kPageLoadInternalSoftNavigationOutcome,
        SoftNavigationOutcome::kNoSoftNavContextDuringUrlChange);
    return;
  }

  if (!context) {
    // All URL changes which follow an attributed URL change are assumed to be
    // client-side-redirects and will not disable paint attribution or change
    // the emitting of existing contexts.
    // TODO(crbug.com/353043684, crbug.com/40943017): Perhaps there should be
    // limits to how long we will keep the current context as active.
    context_for_current_url_->AddUrl(new_url_string,
                                     ToV8NavigationType(load_type),
                                     same_document_metrics_token);

    TRACE_EVENT_INSTANT("loading",
                        "SoftNavigationHeuristics::"
                        "SameDocumentNavigationCommittedWithoutContextButMerg"
                        "edIntoPreviousContext",
                        perfetto::Track::FromPointer(context), "context",
                        *context_for_current_url_, "url", new_url_string);
    base::UmaHistogramEnumeration(
        kPageLoadInternalSoftNavigationOutcome,
        SoftNavigationOutcome::
            kNoSoftNavContextDuringUrlChangeButMergingIntoPreviousContext);
    return;
  }

  context->AddUrl(new_url_string, ToV8NavigationType(load_type),
                  same_document_metrics_token);
  context_for_current_url_ = context;

  TRACE_EVENT_INSTANT(
      "loading", "SoftNavigationHeuristics::SameDocumentNavigationCommitted",
      perfetto::Track::FromPointer(context), "context", *context);

  MaybeCommitNavigationOrEmitSoftNavigation(context);
}

bool SoftNavigationHeuristics::ModifiedDOM(Node* node) {
  // This should only be called by `ModifiedNode()` and `InsertedNode()`, and
  // detached windows should already be filtered out.
  CHECK(window_->GetFrame());

  SoftNavigationContext* context = GetSoftNavigationContextForCurrentTask();
  if (!context) {
    return false;
  }
  paint_attribution_tracker_->MarkNodeAsDirectlyModified(node, context);

  MaybeCommitNavigationOrEmitSoftNavigation(context);
  return true;
}

void SoftNavigationHeuristics::ModifiedAttribute(
    Element* element,
    const QualifiedName& attribute) {
  DCHECK(attribute == html_names::kClassAttr ||
         (attribute == html_names::kStyleAttr && element->IsStyledElement()));
  ModifiedNode(element);
}

void SoftNavigationHeuristics::MaybeCommitNavigationOrEmitSoftNavigation(
    SoftNavigationContext* context) {
  // This is already a soft nav, and the performance entry has already been
  // emitted.
  if (context->WasEmitted()) {
    return;
  }

  // If the navigation ID was set but it hasn't been emitted, then we're waiting
  // on FCP presentation time to emit. If we have that, emit now; otherwise do
  // nothing, since we don't want to count it twice.
  if (context->HasNavigationId()) {
    if (context->HasFirstContentfulPaint()) {
      EmitSoftNavigation(context);
    }
    return;
  }

  // We don't want to Emit for any context except the current URL.
  // If we collect painted area for contexts other than this one, we still don't
  // want to reach "Emit" criteria.
  if (context != context_for_current_url_) {
    return;
  }

  // Are the basic criteria met (interaction, url, dom modification)?
  if (!context->SatisfiesSoftNavNonPaintCriteria()) {
    return;
  }

  // Are we done?
  uint64_t required_paint_area = CalculateRequiredPaintArea();
  if (!context->SatisfiesSoftNavPaintCriteria(required_paint_area)) {
    return;
  }

  // We have met all Soft-Nav criteria!

  // At this point, this navigation should be "committed" to the performance
  // timeline. Thus, we increment the navigation id here, in the animation frame
  // Paint where the criteria are first met. However, the navigation will not be
  // ready for reporting until it also has an FCP measurement.
  // We must *not* wait on this presentation time callback, because all other
  // new performance entries created need to use this new navigation id, in
  // order to match with the eventual soft-nav entry.

  WindowPerformance* performance = DOMWindowPerformance::performance(*window_);
  CHECK(performance);
  performance->IncrementNavigationId();
  context->StartSlicingPerformanceTimeline(
      /*navigation_id=*/performance->NavigationId(),
      /*soft_navigation_offset=*/++soft_navigation_count_,
      /*soft_navigation_slicing_time=*/base::TimeTicks::Now());
  // For metrics reporting, FCP presentation feedback will is in a separate
  // record, when the ICP is reported. Therefore, we can send this immediately,
  // which helps with slicing CLS and INP based on soft_navigation_slicing_time.
  ReportSoftNavigationToMetrics(context);

  // Postpone emitting the entry if we're still waiting for FCP presentation
  // feedback.
  if (!context->HasFirstContentfulPaint()) {
    contexts_waiting_for_paint_timestamp_.insert(context);
    return;
  }
  EmitSoftNavigation(context);
}

void SoftNavigationHeuristics::EmitSoftNavigation(
    SoftNavigationContext* context) {
  context->EmitSoftNavigation();

  // Emitting the entry unblocks reporting the current ICP to metrics, so update
  // metrics now.
  UpdateSoftLcpMetricsForContext(context);
}

SoftNavigationContext*
SoftNavigationHeuristics::MaybeGetSoftNavigationContextForTiming(Node* node) {
  SoftNavigationContext* context =
      paint_attribution_tracker_->GetSoftNavigationContextForNode(node);
  if (!context || !context->IsRecordingLargestContentfulPaint()) {
    return nullptr;
  }
  return context;
}

void SoftNavigationHeuristics::OnPaintFinished() {
  for (const auto& context : interaction_id_to_context_.Values()) {
    if (context->OnPaintFinished()) {
      MaybeCommitNavigationOrEmitSoftNavigation(context);
    }
  }
}

void SoftNavigationHeuristics::OnInputOrScroll() {
  for (const auto& context : interaction_id_to_context_.Values()) {
    // TODO(crbug.com/425402677): Is this is a good time to emit metrics to UKM,
    // and potentially force exhausting the context / remove it from
    // `interaction_id_to_context_`?
    context->OnInputOrScroll();
  }
}

void SoftNavigationHeuristics::OnFramePresented(
    const HeapVector<Member<ImageRecord>>& image_records,
    const HeapVector<Member<TextRecord>>& text_records) {
  // First, group the records by context, ignoring records that aren't needed.
  ContextToCandidatesMap candidates_per_context;
  GroupLcpCandidatesByContext(image_records, candidates_per_context);
  GroupLcpCandidatesByContext(text_records, candidates_per_context);

  // Next, update the LCP candidate and emit an ICP entry for the active
  // context, if any. We do this before unblocking entries waiting for FCP
  // below, since that also emits and updates metrics.
  for (const auto& context_and_records : candidates_per_context) {
    context_and_records.key->OnFramePresented(context_and_records.value);
  }

  // If we're waiting on FCP presentation feedback to emit entries, check if we
  // can emit now.
  if (!contexts_waiting_for_paint_timestamp_.empty()) {
    for (auto& context : contexts_waiting_for_paint_timestamp_) {
      CHECK(!context->WasEmitted());
      MaybeCommitNavigationOrEmitSoftNavigation(context);
    }
    contexts_waiting_for_paint_timestamp_.erase_if(
        [&](const auto& context) { return context->WasEmitted(); });
  }
}

void SoftNavigationHeuristics::UpdateSoftLcpMetricsForContext(
    SoftNavigationContext* context) {
  // We only support updating metrics for the current URL, even if new paints
  // associated with previous interactions are detected.
  if (context != context_for_current_url_) {
    return;
  }

  // LCP candidate information is updated before emitting the soft nav entry to
  // buffer the most recent ICP candidate, in order to capture information at
  // the relevant time. But we don't want to update metrics until the `context`
  // is considered a soft nav.
  if (!context->WasEmitted()) {
    return;
  }

  LocalFrame* frame = window_->GetFrame();
  // We should not be running paint timing callbacks for detached frames.
  CHECK(frame);
  LocalFrameClient* frame_client = frame->Client();
  CHECK(frame_client);
  WindowPerformance* performance = DOMWindowPerformance::performance(*window_);
  CHECK(performance);
  LargestContentfulPaintDetailsForReporting lcp =
      performance->timingForReporting()
          ->PopulateLargestContentfulPaintDetailsForReporting(
              context->LatestLcpDetailsForUkm());
  lcp.soft_navigation_offset = context->SoftNavigationOffset();
  CHECK(lcp.soft_navigation_offset);
  frame_client->DidObserveSoftLargestContentfulPaint(lcp);
}

void SoftNavigationHeuristics::ReportSoftNavigationToMetrics(
    SoftNavigationContext* context) const {
  LocalFrame* frame = window_->GetFrame();
  // We should not be running paint timing callbacks for detached frames.
  CHECK(frame);
  auto* loader = frame->Loader().GetDocumentLoader();
  // This should only be null if the frame was detached.
  CHECK(loader);

  CHECK_EQ(context->GetSoftNavigationHeuristics(), this);

  if (LocalFrameClient* frame_client = frame->Client()) {
    // TODO(crbug.com/490814752): Some tests simulate events with an impossibly
    // small start_time value, which is less than the initial reference time,
    // which makes the duration appear negative.  We cannot report such values
    // without failing expectations (in tests).
    if (context->TimeOrigin() <= loader->GetTiming().ReferenceMonotonicTime()) {
      return;
    }
    blink::SoftNavigationMetricsForReporting metrics = {
        .soft_navigation_offset = context->SoftNavigationOffset(),
        .start_time = loader->GetTiming().MonotonicTimeToPseudoWallTime(
            context->TimeOrigin()),
        .soft_navigation_slicing_time = context->SoftNavigationSlicingTime(),
        .navigation_type =
            ToNavigationTypeForNavigationApi(context->NavigationType()),
        .same_document_metrics_token = context->SameDocumentMetricsToken(),
    };
    // This notifies UKM about this soft navigation.
    frame_client->DidObserveSoftNavigation(metrics);
  }

  // Count "successful soft nav" in histogram
  base::UmaHistogramEnumeration(kPageLoadInternalSoftNavigationOutcome,
                                SoftNavigationOutcome::kSoftNavigationDetected);
}

void SoftNavigationHeuristics::Trace(Visitor* visitor) const {
  visitor->Trace(context_for_current_url_);
  visitor->Trace(window_);
  visitor->Trace(paint_attribution_tracker_);
  visitor->Trace(contexts_waiting_for_paint_timestamp_);
  visitor->Trace(interaction_effects_monitors_);
  visitor->Trace(interaction_id_to_context_);
}

void SoftNavigationHeuristics::OnContextDisposed(
    SoftNavigationContext* context) {
  // This is only called if the context wasn't explicitly shut down, in which
  // case we want to record metrics for it.
  OnSoftNavigationContextWasExhausted(*context, CalculateViewportArea(),
                                      CalculateRequiredPaintArea());
}

std::optional<scheduler::TaskAttributionTracker::TaskScope>
SoftNavigationHeuristics::MaybeCreateTaskScopeForEvent(
    PerformanceEventTiming* entry) {
  CHECK(entry);
  if (!entry->IsInteraction()) {
    return std::nullopt;
  }
  PerformanceTimelineEntryIdInfo interaction_id =
      entry->GetInteractionIdInfo().value();

  // Note: Do not use GetRelevantContext() because we might have a task scope,
  // context, as a continuation, but this event might not be a new interaction,
  // which would have us fall back to that old scope.
  SoftNavigationContext* context =
      GetSoftNavigationContextForInteractionId(interaction_id);

  // TODO(crbug.com/490552221): If context already exists, we should still
  // update it to add this new event timing entry to it, and pick the "best"
  // event timing to represent the ICP/SoftNav timing data.  All events affect
  // the set of continuations that follow, so we may also want to report all
  // event timings with each ICP.
  if (!context) {
    context = MakeGarbageCollected<SoftNavigationContext>(*window_, entry);
    interaction_id_to_context_.insert(interaction_id.id, context);
  }

  auto* tracker =
      scheduler::TaskAttributionTracker::From(window_->GetIsolate());
  if (!tracker) {
    return std::nullopt;
  }

  return tracker->SetTaskStateVariable(context);
}

uint64_t SoftNavigationHeuristics::CalculateViewportArea() const {
  // This should not be called after detach, so neither the frame nor the frame
  // view should be null.
  LocalFrame* frame = window_->GetFrame();
  CHECK(frame);
  LocalFrameView* local_frame_view = frame->View();
  CHECK(local_frame_view);

  static constexpr uint64_t kMinViewportArea = 1;
  uint64_t viewport_area = local_frame_view->GetLayoutSize().Area64();
  return std::max(viewport_area, kMinViewportArea);
}

uint64_t SoftNavigationHeuristics::CalculateRequiredPaintArea() const {
  static constexpr uint64_t kMinRequiredArea = 1;
  constexpr int kSoftNavigationPaintAreaPercentageInPoints = 1;  // 0.01%
  uint64_t viewport_area = CalculateViewportArea();
  uint64_t required_paint_area =
      (viewport_area * kSoftNavigationPaintAreaPercentageInPoints) / 10000;
  if (required_paint_area > kMinRequiredArea) {
    return required_paint_area;
  }
  return kMinRequiredArea;
}

void SoftNavigationHeuristics::ForEachInteractionEffectsMonitor(
    base::FunctionRef<void(InteractionEffectsMonitor&)> callback) {
  for (const auto& monitor : interaction_effects_monitors_) {
    callback(*monitor.Get());
  }
}

void SoftNavigationHeuristics::RegisterInteractionEffectsMonitor(
    InteractionEffectsMonitor* monitor) {
  // This should not be called after detach.
  CHECK(window_->GetFrame());
  auto result = interaction_effects_monitors_.insert(monitor);
  CHECK(result.is_new_entry);
}

void SoftNavigationHeuristics::UnregisterInteractionEffectsMonitor(
    InteractionEffectsMonitor* monitor) {
  // `interaction_effects_monitors_` is cleared on detach, and the observer
  // might be unregistered after that.
  if (!window_->GetFrame()) {
    return;
  }
  auto iter = interaction_effects_monitors_.find(monitor);
  CHECK_NE(iter, interaction_effects_monitors_.end());
  interaction_effects_monitors_.erase(monitor);
}

// static
void SoftNavigationHeuristics::InsertedNode(Node* inserted_node,
                                            Node* container_node) {
  auto* heuristics = GetHeuristicsForNodeIfShouldTrack(*inserted_node);
  if (!heuristics) {
    return;
  }
  // When a child node, which is an HTML element or text node, is modified
  // within a parent (added, moved, etc), mark that child as modified by soft
  // navigation. Otherwise, mark the parent.
  //
  // TODO(crbug.com/416505975): Is this still needed?
  heuristics->ModifiedDOM(inserted_node->IsHTMLElement() ||
                                  inserted_node->IsTextNode()
                              ? inserted_node
                              : container_node);
}

// static
bool SoftNavigationHeuristics::ModifiedNode(Node* node) {
  auto* heuristics = GetHeuristicsForNodeIfShouldTrack(*node);
  if (!heuristics) {
    return false;
  }
  return heuristics->ModifiedDOM(node);
}

// static
void SoftNavigationHeuristics::OnVideoSrcChanged(HTMLVideoElement* element) {
  if (ModifiedNode(element)) {
    if (LayoutObject* object = element->GetLayoutObject()) {
      PaintTimingDetector::NotifyInteractionTriggeredVideoSrcChange(*object);
    }
  }
}

}  // namespace blink
