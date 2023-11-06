// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"

#include "base/logging.h"
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

const size_t SOFT_NAVIGATION_PAINT_AREA_PRECENTAGE = 20;
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
  flag_set_.Clear();
  potential_soft_navigation_task_ids_.clear();
  disposed_soft_navigation_tasks_ = 0;
  soft_navigation_descendant_cache_.clear();
  SetIsTrackingSoftNavigationHeuristicsOnDocument(false);
  did_reset_paints_ = false;
  did_commit_previous_paints_ = false;
  soft_navigation_conditions_met_ = false;
}

void SoftNavigationHeuristics::UserInitiatedInteraction(
    ScriptState* script_state,
    bool is_unfocused_keyboard_event,
    bool is_new_interaction) {
  // Set task ID to the current one.
  initial_interaction_encountered_ = true;
  ThreadScheduler* scheduler = ThreadScheduler::Current();
  DCHECK(scheduler);
  auto* tracker = scheduler->GetTaskAttributionTracker();
  if (!tracker) {
    return;
  }
  ResetHeuristic();
  CHECK(script_state);
  if (is_unfocused_keyboard_event) {
    // TODO(https://crbug.com/1479052): It seems like the callback invocation is
    // creating a task scope for the click event handler, but not for the key
    // handlers. The reason for that is that the key handlers are running inside
    // of an existing task. It's unclear if this situation is only due to our
    // testing infrastructure limitations, or if key events can actually run
    // inside of existing JS tasks in production.
    scheduler::TaskAttributionInfo* task = tracker->RunningTask(script_state);
    if (task) {
      potential_soft_navigation_task_ids_.insert(task->Id().value());
    }
  }
  if (is_new_interaction) {
    user_interaction_timestamp_ = base::TimeTicks::Now();
  }

  tracker->RegisterObserver(this);
  SetIsTrackingSoftNavigationHeuristicsOnDocument(true);
  TRACE_EVENT_INSTANT("scheduler",
                      "SoftNavigationHeuristics::UserInitiatedInteraction");

  ResetPaintsIfNeeded(script_state);
}

bool SoftNavigationHeuristics::IsCurrentTaskDescendantOfClickEventHandler(
    ScriptState* script_state) {
  if (potential_soft_navigation_task_ids_.empty()) {
    return false;
  }
  ThreadScheduler* scheduler = ThreadScheduler::Current();
  DCHECK(scheduler);
  if (scheduler::TaskAttributionTracker* tracker =
          scheduler->GetTaskAttributionTracker()) {
    scheduler::TaskAttributionInfo* task = tracker->RunningTask(script_state);
    if (!task) {
      return false;
    }
    auto cached_result =
        soft_navigation_descendant_cache_.find(task->Id().value());
    if (cached_result != soft_navigation_descendant_cache_.end()) {
      return cached_result->value;
    }
    bool result =
        tracker->HasAncestorInSet(script_state,
                                  potential_soft_navigation_task_ids_, *task) ==
        scheduler::TaskAttributionTracker::AncestorStatus::kAncestor;
    soft_navigation_descendant_cache_.insert(task->Id().value(), result);
    return result;
  }
  return false;
}

// TODO(yoav): We should also reset the heuristic a few seconds after a click
// event handler is done, to reduce potential cycles.
void SoftNavigationHeuristics::ClickEventEnded(ScriptState* script_state) {
  ThreadScheduler* scheduler = ThreadScheduler::Current();
  DCHECK(scheduler);
  auto* tracker = scheduler->GetTaskAttributionTracker();
  if (!tracker) {
    return;
  }
  tracker->UnregisterObserver(this);
  CheckSoftNavigationConditions();
  TRACE_EVENT_INSTANT("scheduler", "SoftNavigationHeuristics::ClickEventEnded");
}

bool SoftNavigationHeuristics::SetFlagIfDescendantAndCheck(
    ScriptState* script_state,
    FlagType type,
    bool run_descendent_check) {
  if (run_descendent_check &&
      !IsCurrentTaskDescendantOfClickEventHandler(script_state)) {
    // A non-descendent URL change should not set the flag.
    return false;
  }
  flag_set_.Put(type);
  CheckSoftNavigationConditions();
  return true;
}

void SoftNavigationHeuristics::SameDocumentNavigationStarted(
    ScriptState* script_state) {
  bool descendant = true;

  auto* tracker = ThreadScheduler::Current()->GetTaskAttributionTracker();
  // If we have no current task when the navigation is started, there's no need
  // to run a descendent check.
  bool run_descendent_check = tracker && tracker->RunningTask(script_state);

  url_ = String();
  if (!SetFlagIfDescendantAndCheck(script_state, FlagType::kURLChange,
                                   run_descendent_check)) {
    ResetHeuristic();
    descendant = false;
  }
  TRACE_EVENT1("scheduler",
               "SoftNavigationHeuristics::SameDocumentNavigationStarted",
               "descendant", descendant);
}
void SoftNavigationHeuristics::SameDocumentNavigationCommitted(
    ScriptState* script_state,
    const String& url) {
  if (!url_.empty()) {
    return;
  }
  url_ = url;
  CheckSoftNavigationConditions();
  TRACE_EVENT1("scheduler",
               "SoftNavigationHeuristics::SameDocumentNavigationCommitted",
               "url", url);
}

bool SoftNavigationHeuristics::ModifiedDOM(ScriptState* script_state) {
  bool descendant = SetFlagIfDescendantAndCheck(
      script_state, FlagType::kMainModification, /*run_descendent_check=*/true);
  TRACE_EVENT1("scheduler", "SoftNavigationHeuristics::ModifiedDOM",
               "descendant", descendant);
  return descendant;
}

void SoftNavigationHeuristics::CheckSoftNavigationConditions() {
  if (flag_set_ != FlagTypeSet::All()) {
    return;
  }
  // The URL is empty when we saw a Same-Document navigation started, but it
  // wasn't yet committed (and hence we may not know the URL just yet).
  if (url_.empty()) {
    return;
  }

  // Here we consider that we've detected a soft navigation.
  soft_navigation_conditions_met_ = true;
}

void SoftNavigationHeuristics::EmitSoftNavigationEntry(LocalFrame* frame) {
  LocalDOMWindow* window = frame->DomWindow();
  CHECK(window);
  ++soft_navigation_count_;
  window->GenerateNewNavigationId();
  auto* performance = DOMWindowPerformance::performance(*window);
  DCHECK(!url_.IsNull());
  performance->AddSoftNavigationEntry(AtomicString(url_),
                                      user_interaction_timestamp_);

  CommitPreviousPaints(frame);
  ResetHeuristic();

  LogAndTraceDetectedSoftNavigation(frame, window, url_,
                                    user_interaction_timestamp_);

  ReportSoftNavigationToMetrics(frame);
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

void SoftNavigationHeuristics::ReportSoftNavigationToMetrics(
    LocalFrame* frame) const {
  auto* loader = frame->Loader().GetDocumentLoader();

  if (!loader) {
    return;
  }

  auto soft_navigation_start_time =
      loader->GetTiming().MonotonicTimeToPseudoWallTime(
          user_interaction_timestamp_);

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
    scheduler::TaskAttributionInfo& task) {
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
  soft_navigation_descendant_cache_.clear();
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

}  // namespace blink
