// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"

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
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

namespace blink {

namespace {

const size_t SOFT_NAVIGATION_PAINT_AREA_PRECENTAGE = 2;
const size_t HUNDRED_PERCENT = 100;

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
  // TODO(yoav): Ensure all callers don't have spurious IsMainFrame checks.
  if (!window.GetFrame()->IsMainFrame()) {
    return nullptr;
  }
  SoftNavigationHeuristics* heuristics =
      Supplement<LocalDOMWindow>::From<SoftNavigationHeuristics>(window);
  if (!heuristics) {
    heuristics = MakeGarbageCollected<SoftNavigationHeuristics>(window);
    ProvideTo(window, heuristics);
  }
  return heuristics;
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
  potential_soft_navigation_task_ids_.clear();
  interaction_task_id_to_interaction_data_.clear();
  last_interaction_task_id_ = 0;
  last_soft_navigation_ancestor_task_ = absl::nullopt;
  disposed_soft_navigation_tasks_ = 0;
  soft_navigation_descendant_cache_.clear();
  SetIsTrackingSoftNavigationHeuristicsOnDocument(false);
  did_reset_paints_ = false;
  did_commit_previous_paints_ = false;
  soft_navigation_conditions_met_ = false;
  pending_interaction_timestamp_ = base::TimeTicks();
}

void SoftNavigationHeuristics::InteractionCallbackCalled(
    ScriptState* script_state,
    EventScopeType type,
    bool is_new_interaction) {
  // TODO(crbug.com/1503284): return early to avoid check failure crashes.
  if (is_new_interaction || !last_interaction_task_id_) {
    if (pending_interaction_timestamp_.is_null()) {
      return;
    }
  }

  // Set task ID to the current one.
  initial_interaction_encountered_ = true;
  ThreadScheduler* scheduler = ThreadScheduler::Current();
  DCHECK(scheduler);
  auto* tracker = scheduler->GetTaskAttributionTracker();
  if (!tracker) {
    return;
  }

  CHECK(script_state);
  scheduler::TaskAttributionInfo* task = tracker->RunningTask(script_state);
  if (!task) {
    // This can happen in test scenarios that trigger input events outside of
    // their regular flow.
    return;
  }

  if (is_new_interaction || !last_interaction_task_id_) {
    CHECK(!pending_interaction_timestamp_.is_null());
    PerInteractionData data;
    data.user_interaction_timestamp = pending_interaction_timestamp_;
    interaction_task_id_to_interaction_data_.insert(task->Id().value(), data);
    last_interaction_task_id_ = task->Id().value();
  } else {
    task_id_to_interaction_task_id_.insert(task->Id().value(),
                                           last_interaction_task_id_);
  }

  tracker->RegisterObserverIfNeeded(this);
  SetIsTrackingSoftNavigationHeuristicsOnDocument(true);
  TRACE_EVENT_INSTANT("scheduler",
                      "SoftNavigationHeuristics::UserInitiatedInteraction");
}

void SoftNavigationHeuristics::UserInitiatedInteraction(
    ScriptState* script_state) {
  // Ensure that paints would be reset, so that paint recording would continue
  // despite the user interaction.
  did_reset_paints_ = false;
  ResetPaintsIfNeeded(script_state);
}

absl::optional<scheduler::TaskAttributionId>
SoftNavigationHeuristics::GetUserInteractionAncestorTaskIfAny(
    ScriptState* script_state) {
  if (potential_soft_navigation_task_ids_.empty()) {
    return absl::nullopt;
  }
  ThreadScheduler* scheduler = ThreadScheduler::Current();
  DCHECK(scheduler);
  if (scheduler::TaskAttributionTracker* tracker =
          scheduler->GetTaskAttributionTracker()) {
    scheduler::TaskAttributionInfo* task = tracker->RunningTask(script_state);
    if (!task) {
      return absl::nullopt;
    }
    auto cached_result =
        soft_navigation_descendant_cache_.find(task->Id().value());
    if (cached_result != soft_navigation_descendant_cache_.end()) {
      return cached_result->value;
    }
    auto ancestor_task_id = tracker->GetAncestorFromSet(
        script_state, potential_soft_navigation_task_ids_, *task);
    soft_navigation_descendant_cache_.insert(task->Id().value(),
                                             ancestor_task_id);
    return ancestor_task_id;
  }
  return absl::nullopt;
}

absl::optional<scheduler::TaskAttributionId>
SoftNavigationHeuristics::SetFlagIfDescendantAndCheck(ScriptState* script_state,
                                                      FlagType type) {
  absl::optional<scheduler::TaskAttributionId> result =
      GetUserInteractionAncestorTaskIfAny(script_state);
  if (!result) {
    // A non-descendent URL change should not set the flag.
    return absl::nullopt;
  }
  PerInteractionData* data = GetCurrentInteractionData(result.value());
  if (!data) {
    return absl::nullopt;
  }
  data->flag_set.Put(type);
  CheckSoftNavigationConditions(*data);
  return result;
}

void SoftNavigationHeuristics::SameDocumentNavigationStarted(
    ScriptState* script_state) {
  last_soft_navigation_ancestor_task_ =
      SetFlagIfDescendantAndCheck(script_state, FlagType::kURLChange);
  TRACE_EVENT1("scheduler",
               "SoftNavigationHeuristics::SameDocumentNavigationStarted",
               "descendant", !!last_soft_navigation_ancestor_task_);
}
void SoftNavigationHeuristics::SameDocumentNavigationCommitted(
    ScriptState* script_state,
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

bool SoftNavigationHeuristics::ModifiedDOM(ScriptState* script_state) {
  bool descendant =
      SetFlagIfDescendantAndCheck(script_state, FlagType::kMainModification)
          .has_value();
  TRACE_EVENT1("scheduler", "SoftNavigationHeuristics::ModifiedDOM",
               "descendant", descendant);
  return descendant;
}

void SoftNavigationHeuristics::CheckSoftNavigationConditions(
    SoftNavigationHeuristics::PerInteractionData& data) {
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

  soft_navigation_interaction_data_ = data;
}

void SoftNavigationHeuristics::EmitSoftNavigationEntry(LocalFrame* frame) {
  LocalDOMWindow* window = frame->DomWindow();
  CHECK(window);
  ++soft_navigation_count_;
  window->GenerateNewNavigationId();
  auto* performance = DOMWindowPerformance::performance(*window);
  DCHECK(!soft_navigation_interaction_data_.url.IsNull());
  performance->AddSoftNavigationEntry(
      AtomicString(soft_navigation_interaction_data_.url),
      soft_navigation_interaction_data_.user_interaction_timestamp);

  CommitPreviousPaints(frame);
  ResetHeuristic();

  LogAndTraceDetectedSoftNavigation(
      frame, window, soft_navigation_interaction_data_.url,
      soft_navigation_interaction_data_.user_interaction_timestamp);

  ReportSoftNavigationToMetrics(frame);
}

SoftNavigationHeuristics::PerInteractionData*
SoftNavigationHeuristics::GetCurrentInteractionData(
    scheduler::TaskAttributionId task_id) {
  // Get interaction ID from task ID
  scheduler::TaskAttributionIdType interaction_task_id = task_id.value();
  auto interaction_it = task_id_to_interaction_task_id_.find(task_id.value());
  if (interaction_it != task_id_to_interaction_task_id_.end()) {
    interaction_task_id = interaction_it->value;
  }
  // Get interaction data from interaction id
  auto data_it =
      interaction_task_id_to_interaction_data_.find(interaction_task_id);
  if (data_it == interaction_task_id_to_interaction_data_.end()) {
    // This can happen when events are triggered out of the expected order. e.g.
    // when we get a keyup event without a keydown event that preceded it. That
    // can happen in tests.
    return nullptr;
  }

  return &data_it->value;
}

// This is called from Text/ImagePaintTimingDetector when a paint is recorded
// there. If the accumulated paints are large enough, a soft navigation entry is
// emitted.
void SoftNavigationHeuristics::RecordPaint(
    LocalFrame* frame,
    uint64_t painted_area,
    bool is_modified_by_soft_navigation) {
  if (is_modified_by_soft_navigation) {
    softnav_painted_area_ += painted_area;
    uint64_t considered_area = std::min(initial_painted_area_, viewport_area_);
    uint64_t paint_threshold =
        considered_area * SOFT_NAVIGATION_PAINT_AREA_PRECENTAGE;
    if (soft_navigation_conditions_met_ &&
        ((softnav_painted_area_ * HUNDRED_PERCENT) > paint_threshold)) {
      EmitSoftNavigationEntry(frame);
    }
  } else if (!initial_interaction_encountered_) {
    initial_painted_area_ += painted_area;
  }
}

void SoftNavigationHeuristics::SetEventParametersAndQueueNestedOnes(
    EventScopeType type,
    bool is_new_interaction,
    bool is_nested) {
  if (is_nested) {
    nested_event_parameters_.push_back(
        EventParameters(is_new_interaction, type));
    current_event_parameters_ = &nested_event_parameters_.back();
  } else {
    top_event_parameters_ = EventParameters(is_new_interaction, type);
    current_event_parameters_ = &top_event_parameters_;
    nested_event_parameters_.clear();
  }

  pending_interaction_timestamp_ =
      (is_new_interaction || !last_interaction_task_id_)
          ? base::TimeTicks::Now()
          : base::TimeTicks();
}

bool SoftNavigationHeuristics::PopNestedEventParametersIfNeeded() {
  if (nested_event_parameters_.empty()) {
    return false;
  }
  nested_event_parameters_.pop_back();
  if (!nested_event_parameters_.empty()) {
    current_event_parameters_ = &nested_event_parameters_.back();
    return true;
  }
  current_event_parameters_ = &top_event_parameters_;
  return true;
}

void SoftNavigationHeuristics::ReportSoftNavigationToMetrics(
    LocalFrame* frame) const {
  auto* loader = frame->Loader().GetDocumentLoader();

  if (!loader) {
    return;
  }

  auto soft_navigation_start_time =
      loader->GetTiming().MonotonicTimeToPseudoWallTime(
          soft_navigation_interaction_data_.user_interaction_timestamp);

  if (soft_navigation_start_time.is_zero()) {
    internal::
        RecordUmaForPageLoadInternalSoftNavigationFromReferenceInvalidTiming(
            soft_navigation_interaction_data_.user_interaction_timestamp,
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
}

void SoftNavigationHeuristics::ResetPaintsIfNeeded(ScriptState* script_state) {
  ScriptState::Scope scope(script_state);
  LocalFrame* frame = ToLocalFrameIfNotDetached(script_state->GetContext());
  if (!frame || !frame->IsOutermostMainFrame()) {
    return;
  }
  LocalDOMWindow* window = frame->DomWindow();
  DCHECK(window);
  if (!did_reset_paints_) {
    LocalFrameView* local_frame_view = frame->View();

    CHECK(local_frame_view);

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
}

void SoftNavigationHeuristics::OnCreateTaskScope(
    scheduler::TaskAttributionInfo& task,
    ScriptState* script_state) {
  CHECK(script_state);
  ThreadScheduler* scheduler = ThreadScheduler::Current();
  CHECK(scheduler);
  auto* tracker = scheduler->GetTaskAttributionTracker();
  if (!tracker) {
    return;
  }
  tracker->SetObserverForTaskDisposal(task.Id(), this);
  // We're inside a click event handler, so need to add this task to the set of
  // potential soft navigation root tasks.
  TRACE_EVENT1("scheduler", "SoftNavigationHeuristics::OnCreateTaskScope",
               "task_id", task.Id().value());
  potential_soft_navigation_task_ids_.insert(task.Id().value());
  if (!pending_interaction_timestamp_.is_null()) {
    PerInteractionData data;
    data.user_interaction_timestamp = pending_interaction_timestamp_;
    interaction_task_id_to_interaction_data_.insert(task.Id().value(), data);
    last_interaction_task_id_ = task.Id().value();
  }
  soft_navigation_descendant_cache_.clear();

  // Create a user initiated interaction
  CHECK(current_event_parameters_);
  InteractionCallbackCalled(script_state, current_event_parameters_->type,
                            current_event_parameters_->is_new_interaction);
  if (current_event_parameters_->type ==
      SoftNavigationHeuristics::EventScopeType::Navigate) {
    SameDocumentNavigationStarted(script_state);
  }
}

void SoftNavigationHeuristics::OnTaskDisposal(
    const scheduler::TaskAttributionInfo& task) {
  if (potential_soft_navigation_task_ids_.Contains(task.Id().value())) {
    if (++disposed_soft_navigation_tasks_ >=
        potential_soft_navigation_task_ids_.size()) {
      // When all the soft navigation tasks were garbage collected, that means
      // that all their descendant tasks are done, and there's no need to
      // continue searching for soft navigation signals, at least not until the
      // next user interaction.
      ResetHeuristic();
    }
  }
}

ExecutionContext* SoftNavigationHeuristics::GetExecutionContext() {
  return GetSupplementable();
}

// SoftNavigationEventScope implementation
// ///////////////////////////////////////////
SoftNavigationEventScope::SoftNavigationEventScope(
    SoftNavigationHeuristics* heuristics,
    ScriptState* script_state,
    SoftNavigationHeuristics::EventScopeType type,
    bool is_new_interaction)
    : heuristics_(heuristics), script_state_(script_state) {
  ThreadScheduler* scheduler = ThreadScheduler::Current();
  DCHECK(scheduler);
  auto* tracker = scheduler->GetTaskAttributionTracker();
  if (!tracker) {
    return;
  }
  // EventScope can be nested in case a click/keyboard event synchronously
  // initiates a navigation.
  bool nested = !tracker->RegisterObserverIfNeeded(heuristics_);

  // Even for nested event scopes, we need to set these parameters, to ensure
  // that created tasks know they were initiated by the correct event type.
  heuristics_->SetEventParametersAndQueueNestedOnes(type, is_new_interaction,
                                                    nested);

  if (!nested) {
    heuristics_->UserInitiatedInteraction(script_state);
  }
}

SoftNavigationEventScope::~SoftNavigationEventScope() {
  bool nested = heuristics_->PopNestedEventParametersIfNeeded();

  // Only the top level EventScope should unregister the observer.
  if (!nested) {
    ThreadScheduler* scheduler = ThreadScheduler::Current();
    DCHECK(scheduler);
    auto* tracker = scheduler->GetTaskAttributionTracker();
    if (!tracker) {
      return;
    }
    tracker->UnregisterObserver(heuristics_);
  }
  // TODO(crbug.com/1502640): We should also reset the heuristic a few seconds
  // after a click event handler is done, to reduce potential cycles.
}
}  // namespace blink
