// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

namespace blink {

namespace {

const size_t SOFT_NAVIGATION_PAINT_AREA_PRECENTAGE = 2;
const size_t HUNDRED_PERCENT = 100;

const char kPageLoadInternalSoftNavigationOutcome[] =
    "PageLoad.Internal.SoftNavigationOutcome";

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SoftNavigationOutcome" in tools/metrics/histograms/enums.xml. Note also that
// these form a bitmask; future conditions should continue this pattern.
// LINT.IfChange
enum SoftNavigationOutcome {
  kSoftNavigationDetected = 0,
  kNoAncestorTask = 1,
  kNoPaint = 2,
  kNoAncestorTaskOrPaint = 3,
  kNoDomModification = 4,
  kNoAncestorOrDomModification = 5,
  kNoPaintOrDomModification = 6,
  kNoConditionsMet = 7,
  kMaxValue = kNoConditionsMet,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:SoftNavigationOutcome)

void LogAndTraceDetectedSoftNavigation(LocalFrame* frame,
                                       LocalDOMWindow* window,
                                       String url,
                                       base::TimeTicks user_click_timestamp) {
  CHECK(frame && frame->IsMainFrame());
  CHECK(window);
  if (!RuntimeEnabledFeatures::SoftNavigationHeuristicsEnabled(window)) {
    return;
  }
  auto* console_message = MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kInfo,
      String("A soft navigation has been detected: ") + url);
  window->AddConsoleMessage(console_message);

  TRACE_EVENT_INSTANT("scheduler,devtools.timeline,loading",
                      "SoftNavigationHeuristics_SoftNavigationDetected",
                      user_click_timestamp, "frame",
                      GetFrameIdForTracing(frame), "url", url, "navigationId",
                      window->GetNavigationId());
}
}  // namespace

namespace internal {
void RecordUmaForPageLoadInternalSoftNavigationFromReferenceInvalidTiming(
    base::TimeTicks user_interaction_ts,
    base::TimeTicks reference_ts) {
  if (user_interaction_ts.is_null()) {
    if (reference_ts.is_null()) {
      base::UmaHistogramEnumeration(
          kPageLoadInternalSoftNavigationFromReferenceInvalidTiming,
          SoftNavigationFromReferenceInvalidTimingReasons::
              kUserInteractionTsAndReferenceTsBothNull);
    } else {
      base::UmaHistogramEnumeration(
          kPageLoadInternalSoftNavigationFromReferenceInvalidTiming,
          SoftNavigationFromReferenceInvalidTimingReasons::
              kNullUserInteractionTsAndNotNullReferenceTs);
    }
  } else {
    if (reference_ts.is_null()) {
      base::UmaHistogramEnumeration(
          kPageLoadInternalSoftNavigationFromReferenceInvalidTiming,
          SoftNavigationFromReferenceInvalidTimingReasons::
              kNullReferenceTsAndNotNullUserInteractionTs);

    } else {
      base::UmaHistogramEnumeration(
          kPageLoadInternalSoftNavigationFromReferenceInvalidTiming,
          SoftNavigationFromReferenceInvalidTimingReasons::
              kUserInteractionTsAndReferenceTsBothNotNull);
    }
  }
}

}  // namespace internal

// static
const char SoftNavigationHeuristics::kSupplementName[] =
    "SoftNavigationHeuristics";

SoftNavigationHeuristics::SoftNavigationHeuristics(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {
  LocalFrame* frame = window.GetFrame();
  CHECK(frame && frame->View());
  gfx::Size viewport_size = frame->View()->GetLayoutSize();
  viewport_area_ = viewport_size.width() * viewport_size.height();
}

SoftNavigationHeuristics* SoftNavigationHeuristics::From(
    LocalDOMWindow& window) {
  if (!window.GetFrame()->IsMainFrame()) {
    return nullptr;
  }
  SoftNavigationHeuristics* heuristics =
      Supplement<LocalDOMWindow>::From<SoftNavigationHeuristics>(window);
  if (!heuristics) {
    if (Document* document = window.document()) {
      // Don't measure soft navigations in devtools.
      if (document->Url().ProtocolIs("devtools")) {
        return nullptr;
      }
    }
    heuristics = MakeGarbageCollected<SoftNavigationHeuristics>(window);
    ProvideTo(window, heuristics);
  }
  return heuristics;
}

void SoftNavigationHeuristics::Dispose() {
  if (has_potential_soft_navigation_task_ &&
      potential_soft_navigation_tasks_.empty()) {
    RecordUmaForNonSoftNavigationInteractions();
  }
}

void SoftNavigationHeuristics::RecordUmaForNonSoftNavigationInteractions()
    const {
  for (const auto& task_id_and_interaction_data :
       interaction_task_id_to_interaction_data_) {
    const PerInteractionData& data = *task_id_and_interaction_data.value;

    // For all interactions which included a URL modification, log the
    // criteria which were not met. Note that we assume here that an ancestor
    // task was found when the URL change was made.
    if (data.flag_set.Has(kURLChange)) {
      if (!data.flag_set.Has(FlagType::kMainModification)) {
        if (!paint_conditions_met_) {
          base::UmaHistogramEnumeration(
              kPageLoadInternalSoftNavigationOutcome,
              SoftNavigationOutcome::kNoDomModification);
        } else {
          base::UmaHistogramEnumeration(
              kPageLoadInternalSoftNavigationOutcome,
              SoftNavigationOutcome::kNoPaintOrDomModification);
        }
      } else if (!paint_conditions_met_) {
        base::UmaHistogramEnumeration(kPageLoadInternalSoftNavigationOutcome,
                                      SoftNavigationOutcome::kNoPaint);
      }
    }
  }
}

void SoftNavigationHeuristics::SetIsTrackingSoftNavigationHeuristicsOnDocument(
    bool value) const {
  LocalDOMWindow* window = GetSupplementable();
  if (!window) {
    return;
  }
  if (Document* document = window->document()) {
    document->SetIsTrackingSoftNavigationHeuristics(value);
  }
}

void SoftNavigationHeuristics::ResetHeuristic() {
  // Reset previously seen indicators and task IDs.
  has_potential_soft_navigation_task_ = false;
  potential_soft_navigation_tasks_.clear();
  interaction_task_id_to_interaction_data_.clear();
  soft_navigation_interaction_data_ = nullptr;
  last_interaction_task_id_ = scheduler::TaskAttributionId();
  last_soft_navigation_ancestor_task_ = std::nullopt;
  soft_navigation_descendant_cache_.clear();
  SetIsTrackingSoftNavigationHeuristicsOnDocument(false);
  did_reset_paints_ = false;
  did_commit_previous_paints_ = false;
  soft_navigation_conditions_met_ = false;
  pending_interaction_timestamp_ = base::TimeTicks();
  paint_conditions_met_ = false;
  softnav_painted_area_ = 0;
}

void SoftNavigationHeuristics::UserInitiatedInteraction() {
  // Ensure that paints would be reset, so that paint recording would continue
  // despite the user interaction.
  did_reset_paints_ = false;
  ResetPaintsIfNeeded();
}

std::optional<scheduler::TaskAttributionId>
SoftNavigationHeuristics::GetUserInteractionAncestorTaskIfAny() {
  using IterationStatus = scheduler::TaskAttributionTracker::IterationStatus;

  if (potential_soft_navigation_tasks_.empty()) {
    return std::nullopt;
  }
  if (auto* tracker = scheduler::TaskAttributionTracker::From(
          GetSupplementable()->GetIsolate())) {
    scheduler::TaskAttributionInfo* task = tracker->RunningTask();
    if (!task) {
      return std::nullopt;
    }
    auto cached_result =
        soft_navigation_descendant_cache_.find(task->Id().value());
    if (cached_result != soft_navigation_descendant_cache_.end()) {
      return cached_result->value;
    }
    std::optional<scheduler::TaskAttributionId> ancestor_task_id;
    // Check if any of `potential_soft_navigation_tasks_` is an ancestor of
    // `task`.
    tracker->ForEachAncestor(
        *task, [&](const scheduler::TaskAttributionInfo& ancestor) {
          if (potential_soft_navigation_tasks_.Contains(&ancestor)) {
            ancestor_task_id = ancestor.Id();
            return IterationStatus::kStop;
          }
          return IterationStatus::kContinue;
        });
    soft_navigation_descendant_cache_.insert(task->Id().value(),
                                             ancestor_task_id);
    return ancestor_task_id;
  }
  return std::nullopt;
}

std::optional<scheduler::TaskAttributionId>
SoftNavigationHeuristics::SetFlagIfDescendantAndCheck(FlagType type) {
  std::optional<scheduler::TaskAttributionId> result =
      GetUserInteractionAncestorTaskIfAny();
  if (!result) {
    // A non-descendent URL change should not set the flag.
    return std::nullopt;
  }
  PerInteractionData* data = GetCurrentInteractionData(result.value());
  if (!data) {
    return std::nullopt;
  }
  data->flag_set.Put(type);
  CheckSoftNavigationConditions(*data);
  return result;
}

void SoftNavigationHeuristics::SameDocumentNavigationStarted() {
  last_soft_navigation_ancestor_task_ =
      SetFlagIfDescendantAndCheck(FlagType::kURLChange);
  if (!last_soft_navigation_ancestor_task_) {
    base::UmaHistogramEnumeration(kPageLoadInternalSoftNavigationOutcome,
                                  SoftNavigationOutcome::kNoAncestorTask);
  }
  TRACE_EVENT1("scheduler",
               "SoftNavigationHeuristics::SameDocumentNavigationStarted",
               "descendant", !!last_soft_navigation_ancestor_task_);
}
void SoftNavigationHeuristics::SameDocumentNavigationCommitted(
    const String& url) {
  if (!last_soft_navigation_ancestor_task_) {
    return;
  }
  PerInteractionData* data =
      GetCurrentInteractionData(last_soft_navigation_ancestor_task_.value());
  if (!data) {
    return;
  }
  // This is overriding the URL, which is required to support history
  // modifications inside a popstate event.
  data->url = url;
  CheckSoftNavigationConditions(*data);
  TRACE_EVENT1("scheduler",
               "SoftNavigationHeuristics::SameDocumentNavigationCommitted",
               "url", url);
}

bool SoftNavigationHeuristics::ModifiedDOM() {
  bool descendant =
      SetFlagIfDescendantAndCheck(FlagType::kMainModification).has_value();
  TRACE_EVENT1("scheduler", "SoftNavigationHeuristics::ModifiedDOM",
               "descendant", descendant);
  return descendant;
}

void SoftNavigationHeuristics::CheckSoftNavigationConditions(
    const SoftNavigationHeuristics::PerInteractionData& data) {
  if (data.flag_set != FlagTypeSet::All()) {
    return;
  }
  // The URL is empty when we saw a Same-Document navigation started, but it
  // wasn't yet committed (and hence we may not know the URL just yet).
  if (data.url.empty()) {
    return;
  }

  // Here we consider that we've detected a soft navigation.
  soft_navigation_conditions_met_ = true;
  soft_navigation_interaction_data_ = &data;

  EmitSoftNavigationEntryIfAllConditionsMet(GetLocalFrameIfNotDetached());
}

void SoftNavigationHeuristics::EmitSoftNavigationEntryIfAllConditionsMet(
    LocalFrame* frame) {
  // TODO(crbug.com/1510706): See if we need to add `paint_conditions_met_` back
  // into this condition.
  if (!soft_navigation_conditions_met_ || !soft_navigation_interaction_data_ ||
      soft_navigation_interaction_data_->url.IsNull() ||
      soft_navigation_interaction_data_->user_interaction_timestamp.is_null() ||
      !frame || !frame->IsOutermostMainFrame()) {
    return;
  }
  LocalDOMWindow* window = frame->DomWindow();
  CHECK(window);
  ++soft_navigation_count_;
  window->GenerateNewNavigationId();
  auto* performance = DOMWindowPerformance::performance(*window);
  performance->AddSoftNavigationEntry(
      AtomicString(soft_navigation_interaction_data_->url),
      soft_navigation_interaction_data_->user_interaction_timestamp);

  CommitPreviousPaints(frame);

  LogAndTraceDetectedSoftNavigation(
      frame, window, soft_navigation_interaction_data_->url,
      soft_navigation_interaction_data_->user_interaction_timestamp);

  ReportSoftNavigationToMetrics(frame);
  ResetHeuristic();
}

SoftNavigationHeuristics::PerInteractionData*
SoftNavigationHeuristics::GetCurrentInteractionData(
    scheduler::TaskAttributionId task_id) {
  // Get interaction ID from task ID
  auto interaction_it = task_id_to_interaction_task_id_.find(task_id.value());
  if (interaction_it != task_id_to_interaction_task_id_.end()) {
    task_id = scheduler::TaskAttributionId(interaction_it->value);
  }
  // Get interaction data from interaction id
  auto data_it = interaction_task_id_to_interaction_data_.find(task_id.value());
  if (data_it == interaction_task_id_to_interaction_data_.end()) {
    // This can happen when events are triggered out of the expected order. e.g.
    // when we get a keyup event without a keydown event that preceded it. That
    // can happen in tests.
    return nullptr;
  }

  return data_it->value.Get();
}

// This is called from Text/ImagePaintTimingDetector when a paint is recorded
// there.
void SoftNavigationHeuristics::RecordPaint(
    LocalFrame* frame,
    uint64_t painted_area,
    bool is_modified_by_soft_navigation) {
  if (!initial_interaction_encountered_ && is_modified_by_soft_navigation) {
    // TODO(crbug.com/41496928): Paints can be reported for Nodes which had
    // is_modified... flag set but a different instance of a
    // SoftNavigationHeuristics class.  This happens when Nodes are re-parented
    // into a new document, e.g. into an open() window.
    // Instead of just ignoring the worst case of this issue as we do here, we
    // should support this use case.  Either by clearing the flag on nodes, or,
    // by staring an interaction/navigation id on Node, rathan than boolean.
    return;
  }
  if (!initial_interaction_encountered_) {
    // We haven't seen an interaction yet, so we are still measuring initial
    // paint area.
    CHECK(!is_modified_by_soft_navigation);
    CHECK(!has_potential_soft_navigation_task_);
    initial_painted_area_ += painted_area;
    return;
  }

  if (!has_potential_soft_navigation_task_) {
    // We aren't measuring a soft-nav so we can just exit.
    return;
  }

  if (!is_modified_by_soft_navigation) {
    return;
  }

  softnav_painted_area_ += painted_area;

  uint64_t required_paint_area =
      std::min(initial_painted_area_, viewport_area_);

  if (required_paint_area == 0) {
    return;
  }

  float softnav_painted_area_ratio =
      (float)softnav_painted_area_ / (float)required_paint_area;

  uint64_t required_paint_area_scaled =
      required_paint_area * SOFT_NAVIGATION_PAINT_AREA_PRECENTAGE;
  uint64_t softnav_painted_area_scaled =
      softnav_painted_area_ * HUNDRED_PERCENT;
  bool is_above_threshold =
      (softnav_painted_area_scaled > required_paint_area_scaled);

  TRACE_EVENT_INSTANT("loading", "SoftNavigationHeuristics_RecordPaint",
                      "softnav_painted_area", softnav_painted_area_,
                      "softnav_painted_area_ratio", softnav_painted_area_ratio,
                      "url",
                      (soft_navigation_interaction_data_
                           ? soft_navigation_interaction_data_->url
                           : ""),
                      "is_above_threshold", is_above_threshold);

  if (is_above_threshold) {
    paint_conditions_met_ = true;
    EmitSoftNavigationEntryIfAllConditionsMet(frame);
  }
}

void SoftNavigationHeuristics::SetCurrentTimeAsStartTime() {
  // The interaction timestamp for non-"new interactions" will be be set to the
  // processing-end time of the associated "new interaction" event, either via
  // `pending_interaction_timestamp_` (if the "new interaction" event didn't
  // have an event listener) or by resuing the `PerInteractionData` from that
  // interaction.
  //
  // Note: kNavigate `EventScope`s considered new interactions even though they
  // may be nested within an existing new interaction. This causes the
  // interaction timestamp to be set to the end of the navigate event
  // processing, which is intended.
  if (!CurrentEventParameters().is_new_interaction) {
    return;
  }
  if (!last_interaction_task_id_.value()) {
    pending_interaction_timestamp_ = base::TimeTicks::Now();
    return;
  }
  PerInteractionData* data =
      GetCurrentInteractionData(last_interaction_task_id_);
  CHECK(data);
  if (data->user_interaction_timestamp.is_null()) {
    // Only set the timestamp if it wasn't previously set, otherwise in the case
    // of nested `EventScope`s (e.g. navigate event within a click event) the
    // the timestamp set at the end of the navigate event processing would be
    // overwritten.
    data->user_interaction_timestamp = base::TimeTicks::Now();
  }
  LocalFrame* frame = GetLocalFrameIfNotDetached();
  EmitSoftNavigationEntryIfAllConditionsMet(frame);
}

void SoftNavigationHeuristics::ReportSoftNavigationToMetrics(
    LocalFrame* frame) const {
  auto* loader = frame->Loader().GetDocumentLoader();

  if (!loader) {
    return;
  }

  CHECK(
      !soft_navigation_interaction_data_->user_interaction_timestamp.is_null());
  auto soft_navigation_start_time =
      loader->GetTiming().MonotonicTimeToPseudoWallTime(
          soft_navigation_interaction_data_->user_interaction_timestamp);

  if (soft_navigation_start_time.is_zero()) {
    internal::
        RecordUmaForPageLoadInternalSoftNavigationFromReferenceInvalidTiming(
            soft_navigation_interaction_data_->user_interaction_timestamp,
            loader->GetTiming().ReferenceMonotonicTime());
  }

  LocalDOMWindow* window = frame->DomWindow();

  CHECK(window);

  blink::SoftNavigationMetrics metrics = {soft_navigation_count_,
                                          soft_navigation_start_time,
                                          window->GetNavigationId().Utf8()};

  if (LocalFrameClient* frame_client = frame->Client()) {
    // This notifies UKM about this soft navigation.
    frame_client->DidObserveSoftNavigation(metrics);
  }

  // Count "successful soft nav" in histogram
  base::UmaHistogramEnumeration(kPageLoadInternalSoftNavigationOutcome,
                                SoftNavigationOutcome::kSoftNavigationDetected);
}

void SoftNavigationHeuristics::ResetPaintsIfNeeded() {
  LocalFrame* frame = GetLocalFrameIfNotDetached();
  if (!frame || !frame->IsOutermostMainFrame()) {
    return;
  }
  if (!did_reset_paints_) {
    LocalFrameView* local_frame_view = frame->View();
    CHECK(local_frame_view);
    LocalDOMWindow* window = GetSupplementable();
    if (RuntimeEnabledFeatures::SoftNavigationHeuristicsEnabled(window)) {
      if (Document* document = window->document();
          document &&
          RuntimeEnabledFeatures::SoftNavigationHeuristicsExposeFPAndFCPEnabled(
              window)) {
        PaintTiming::From(*document).ResetFirstPaintAndFCP();
      }
      local_frame_view->GetPaintTimingDetector().RestartRecordingLCP();
    }

    local_frame_view->GetPaintTimingDetector().RestartRecordingLCPToUkm();

    did_reset_paints_ = true;
  }
}

// Once all the soft navigation conditions are met (verified in
// CheckSoftNavigationConditions), the previous paints are committed, to make
// sure accumulated FP, FCP and LCP entries are properly fired.
void SoftNavigationHeuristics::CommitPreviousPaints(LocalFrame* frame) {
  if (!frame || !frame->IsOutermostMainFrame()) {
    return;
  }
  LocalDOMWindow* window = frame->DomWindow();
  CHECK(window);
  if (!did_commit_previous_paints_) {
    LocalFrameView* local_frame_view = frame->View();

    CHECK(local_frame_view);

    local_frame_view->GetPaintTimingDetector().SoftNavigationDetected(window);
    if (RuntimeEnabledFeatures::SoftNavigationHeuristicsExposeFPAndFCPEnabled(
            window)) {
      PaintTiming::From(*window->document()).SoftNavigationDetected();
    }

    did_commit_previous_paints_ = true;
  }
}

void SoftNavigationHeuristics::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(potential_soft_navigation_tasks_);
  visitor->Trace(interaction_task_id_to_interaction_data_);
  visitor->Trace(soft_navigation_interaction_data_);
  // Register a custom weak callback, which runs after processing weakness for
  // the container. This allows us to observe the collection becoming empty
  // without needing to observe individual element disposal.
  visitor->RegisterWeakCallbackMethod<
      SoftNavigationHeuristics,
      &SoftNavigationHeuristics::ProcessCustomWeakness>(this);
}

void SoftNavigationHeuristics::OnCreateTaskScope(
    scheduler::TaskAttributionInfo& task) {
  TRACE_EVENT1("scheduler", "SoftNavigationHeuristics::OnCreateTaskScope",
               "task_id", task.Id().value());
  // This is invoked when executing a callback with an active `EventScope`,
  // which happens for click and keyboard input events, as well as
  // user-initiated navigation and popstate events. Any such events should be
  // considered a potential soft navigation root tasks.
  potential_soft_navigation_tasks_.insert(&task);
  has_potential_soft_navigation_task_ = true;

  const EventParameters& current_event_parameters = CurrentEventParameters();
  // If `last_interaction_task_id_` isn't set, then no event listeners for any
  // associated events have run yet -- either in the intital "new interaction"
  // `EventScope`, a nested `EventScope`, or a subsequent non-"new interaction"
  // (e.g. keyup) `EventScope`. In that case, no `PerInteractionData` data has
  // been created for the current interaction, so create one now that the
  // interaction is a potential soft navigation.
  //
  // Note: multiple event listeners might within an `EventScope`, but the
  // `last_interaction_task_id_` will only be set for the first one, and only if
  // `last_interaction_task_id_` wasn't already set.
  if (!last_interaction_task_id_.value()) {
    PerInteractionData* data = MakeGarbageCollected<PerInteractionData>();
    if (!current_event_parameters.is_new_interaction) {
      // The `PerInteractionData` wasn't created for the "new interaction", but
      // we still want to use the processing-end timestamp from that event.
      data->user_interaction_timestamp = pending_interaction_timestamp_;
    }
    interaction_task_id_to_interaction_data_.insert(task.Id().value(), data);
    last_interaction_task_id_ = task.Id();
  } else {
    task_id_to_interaction_task_id_.insert(task.Id().value(),
                                           last_interaction_task_id_.value());
  }

  initial_interaction_encountered_ = true;
  SetIsTrackingSoftNavigationHeuristicsOnDocument(true);
  soft_navigation_descendant_cache_.clear();

  if (current_event_parameters.type == EventScope::Type::kNavigate) {
    SameDocumentNavigationStarted();
  }
}

void SoftNavigationHeuristics::ProcessCustomWeakness(
    const LivenessBroker& info) {
  // When all the soft navigation tasks were garbage collected, that means that
  // all their descendant tasks are done, and there's no need to continue
  // searching for soft navigation signals, at least not until the next user
  // interaction.
  //
  // Note: This is not allowed to do Oilpan allocations. If that's needed, this
  // can schedule a task or microtask to reset the heuristic.
  if (has_potential_soft_navigation_task_ &&
      potential_soft_navigation_tasks_.empty()) {
    RecordUmaForNonSoftNavigationInteractions();
    ResetHeuristic();
  }
}

LocalFrame* SoftNavigationHeuristics::GetLocalFrameIfNotDetached() const {
  LocalDOMWindow* window = GetSupplementable();
  return window->IsCurrentlyDisplayedInFrame() ? window->GetFrame() : nullptr;
}

SoftNavigationHeuristics::EventScope SoftNavigationHeuristics::CreateEventScope(
    EventScope::Type type,
    bool is_new_interaction) {
  // Even for nested event scopes, we need to set these parameters, to ensure
  // that created tasks know they were initiated by the correct event type.
  all_event_parameters_.push_back(EventParameters(is_new_interaction, type));

  if (all_event_parameters_.size() == 1) {
    UserInitiatedInteraction();
    // Clear the state needed to link multiple events together (e.g. keydown and
    // keyup)  so we don't inadvertently link a new interaction with an old one.
    // Only doing this for the outermost `EventScope` will cause nested scopes
    // to be considered part of the same interaction.
    if (is_new_interaction) {
      last_interaction_task_id_ = scheduler::TaskAttributionId();
      pending_interaction_timestamp_ = base::TimeTicks();
    }
  }

  return SoftNavigationHeuristics::EventScope(
      this, scheduler::TaskAttributionTracker::From(
                GetSupplementable()->GetIsolate()));
}

void SoftNavigationHeuristics::OnSoftNavigationEventScopeDestroyed() {
  // Set the start time to the end of event processing. In case of nested event
  // scopes, we want this to be the end of the nested `navigate()` event
  // handler.
  SetCurrentTimeAsStartTime();

  // `SetCurrentTimeAsStartTime()` depends on `CurrentEventParameters()`, so
  // clear this last.
  all_event_parameters_.pop_back();

  // TODO(crbug.com/1502640): We should also reset the heuristic a few seconds
  // after a click event handler is done, to reduce potential cycles.
}

// SoftNavigationHeuristics::EventScope implementation
// ///////////////////////////////////////////
SoftNavigationHeuristics::EventScope::EventScope(
    SoftNavigationHeuristics* heuristics,
    scheduler::TaskAttributionTracker* tracker)
    : heuristics_(heuristics) {
  CHECK(heuristics_);
  if (tracker) {
    observer_scope_ = tracker->RegisterObserver(heuristics_);
  }
}

SoftNavigationHeuristics::EventScope::EventScope(EventScope&& other)
    : heuristics_(std::exchange(other.heuristics_, nullptr)),
      observer_scope_(std::move(other.observer_scope_)) {}

SoftNavigationHeuristics::EventScope&
SoftNavigationHeuristics::EventScope::operator=(EventScope&& other) {
  heuristics_ = std::exchange(other.heuristics_, nullptr);
  observer_scope_ = std::move(other.observer_scope_);
  return *this;
}

SoftNavigationHeuristics::EventScope::~EventScope() {
  if (!heuristics_) {
    return;
  }
  heuristics_->OnSoftNavigationEventScopeDestroyed();
}

}  // namespace blink
