// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"

#include "base/logging.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

namespace blink {

namespace {

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
                      GetFrameIdForTracing(frame), "url", url);
}

}  // namespace

// static
const char SoftNavigationHeuristics::kSupplementName[] =
    "SoftNavigationHeuristics";

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
  SetIsTrackingSoftNavigationHeuristicsOnDocument(false);
  did_reset_paints_ = false;
}

void SoftNavigationHeuristics::UserInitiatedClick(ScriptState* script_state) {
  // Set task ID to the current one.
  ThreadScheduler* scheduler = ThreadScheduler::Current();
  DCHECK(scheduler);
  // This should not be called off-main-thread.
  DCHECK(scheduler->GetTaskAttributionTracker());
  ResetHeuristic();
  scheduler->GetTaskAttributionTracker()->RegisterObserver(this);
  SetIsTrackingSoftNavigationHeuristicsOnDocument(true);
  user_click_timestamp_ = base::TimeTicks::Now();
  TRACE_EVENT_INSTANT("scheduler",
                      "SoftNavigationHeuristics::UserInitiatedClick");
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
    return (tracker->HasAncestorInSet(script_state,
                                      potential_soft_navigation_task_ids_) ==
            scheduler::TaskAttributionTracker::AncestorStatus::kAncestor);
  }
  return false;
}

// TODO(yoav): We should also reset the heuristic a few seconds after a click
// event handler is done, to reduce potential cycles.
void SoftNavigationHeuristics::ClickEventEnded(ScriptState* script_state) {
  ThreadScheduler* scheduler = ThreadScheduler::Current();
  DCHECK(scheduler);
  scheduler->GetTaskAttributionTracker()->UnregisterObserver(this);
  CheckAndReportSoftNavigation(script_state);
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
  CheckAndReportSoftNavigation(script_state);
  return true;
}

void SoftNavigationHeuristics::SameDocumentNavigationStarted(
    ScriptState* script_state) {
  bool descendant = true;

  auto* tracker = ThreadScheduler::Current()->GetTaskAttributionTracker();
  // If we have no current task when the navigation is started, there's no need
  // to run a descendent check.
  bool run_descendent_check =
      tracker && tracker->RunningTaskAttributionId(script_state);

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
  CheckAndReportSoftNavigation(script_state);
  TRACE_EVENT1("scheduler",
               "SoftNavigationHeuristics::SameDocumentNavigationCommitted",
               "url", url);
}

void SoftNavigationHeuristics::ModifiedDOM(ScriptState* script_state) {
  bool descendant = SetFlagIfDescendantAndCheck(
      script_state, FlagType::kMainModification, /*run_descendent_check=*/true);
  TRACE_EVENT1("scheduler", "SoftNavigationHeuristics::ModifiedDOM",
               "descendant", descendant);
  // TODO(https://crbug.com/1430009): This is racy. We should figure out another
  // point in time to stop the heuristic.
  SetIsTrackingSoftNavigationHeuristicsOnDocument(false);
}

void SoftNavigationHeuristics::CheckAndReportSoftNavigation(
    ScriptState* script_state) {
  if (flag_set_ != FlagTypeSet::All()) {
    return;
  }
  ScriptState::Scope scope(script_state);
  LocalFrame* frame = ToLocalFrameIfNotDetached(script_state->GetContext());
  if (!frame || !frame->IsMainFrame()) {
    return;
  }
  LocalDOMWindow* window = frame->DomWindow();
  DCHECK(window);
  // The URL is empty when we saw a Same-Document navigation started, but it
  // wasn't yet committed (and hence we may not know the URL just yet).
  if (url_.empty()) {
    ResetPaintsIfNeeded(frame, window);
    return;
  }
  ++soft_navigation_count_;
  window->IncrementNavigationId();
  auto* performance = DOMWindowPerformance::performance(*window);
  DCHECK(!url_.IsNull());
  performance->AddSoftNavigationEntry(AtomicString(url_),
                                      user_click_timestamp_);

  // TODO(yoav): There's a theoretical race here where DOM modifications trigger
  // paints before the URL change happens, leading to unspotted LCPs and FCPs.
  ResetPaintsIfNeeded(frame, window);

  ResetHeuristic();
  LogAndTraceDetectedSoftNavigation(frame, window, url_, user_click_timestamp_);
  if (LocalFrameClient* frame_client = frame->Client()) {
    // This notifies UKM about this soft navigation.
    frame_client->DidObserveSoftNavigation(soft_navigation_count_);
  }
}

void SoftNavigationHeuristics::ResetPaintsIfNeeded(LocalFrame* frame,
                                                   LocalDOMWindow* window) {
  if (!did_reset_paints_) {
    if (RuntimeEnabledFeatures::SoftNavigationHeuristicsEnabled(window)) {
      if (Document* document = window->document()) {
        PaintTiming::From(*document).ResetFirstPaintAndFCP();
      }
      DCHECK(frame->View());
      frame->View()->GetPaintTimingDetector().RestartRecordingLCP();
    }
    did_reset_paints_ = true;
  }
}

void SoftNavigationHeuristics::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
}

void SoftNavigationHeuristics::OnCreateTaskScope(
    const scheduler::TaskAttributionId& task_id) {
  // We're inside a click event handler, so need to add this task to the set of
  // potential soft navigation root tasks.
  TRACE_EVENT1("scheduler", "SoftNavigationHeuristics::OnCreateTaskScope",
               "task_id", task_id.value());
  potential_soft_navigation_task_ids_.insert(task_id.value());
}

ExecutionContext* SoftNavigationHeuristics::GetExecutionContext() {
  return GetSupplementable();
}

}  // namespace blink
