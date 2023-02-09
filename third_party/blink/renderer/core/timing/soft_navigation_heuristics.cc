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

void LogToConsole(LocalFrame* frame,
                  mojom::blink::ConsoleMessageLevel level,
                  const String& message) {
  if (!frame || !frame->IsMainFrame()) {
    return;
  }
  LocalDOMWindow* window = frame->DomWindow();
  if (!window) {
    return;
  }
  if (!RuntimeEnabledFeatures::SoftNavigationHeuristicsEnabled(window)) {
    return;
  }
  auto* console_message = MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript, level, message);
  window->AddConsoleMessage(console_message);
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
    absl::optional<String> url,
    bool skip_descendant_check) {
  if (!skip_descendant_check &&
      !IsCurrentTaskDescendantOfClickEventHandler(script_state)) {
    // A non-descendent URL change should not set the flag.
    return false;
  }
  flag_set_.Put(type);
  if (url) {
    url_ = *url;
  }
  CheckAndReportSoftNavigation(script_state);
  return true;
}

void SoftNavigationHeuristics::SawURLChange(ScriptState* script_state,
                                            const String& url,
                                            bool skip_descendant_check) {
  bool descendant = true;
  if (!SetFlagIfDescendantAndCheck(script_state, FlagType::kURLChange, url,
                                   skip_descendant_check)) {
    ResetHeuristic();
    descendant = false;
  }
  TRACE_EVENT1("scheduler", "SoftNavigationHeuristics::SawURLChange",
               "descendant", descendant);
}

void SoftNavigationHeuristics::ModifiedDOM(ScriptState* script_state) {
  bool descendant =
      SetFlagIfDescendantAndCheck(script_state, FlagType::kMainModification);
  TRACE_EVENT1("scheduler", "SoftNavigationHeuristics::ModifiedDOM",
               "descendant", descendant);
  SetIsTrackingSoftNavigationHeuristicsOnDocument(false);
}

void SoftNavigationHeuristics::SetBackForwardNavigationURL(
    ScriptState* script_state,
    const String& url) {
  if (!url_.empty()) {
    return;
  }
  url_ = url;
  CheckAndReportSoftNavigation(script_state);
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
  // In case of a Soft Navigation using `history.back()`, `history.forward()` or
  // `history.go()`, `SawURLChange` was called with an empty URL. If that's the
  // case, don't report the Soft Navigation just yet, and wait for
  // `SetBackForwardNavigationURL` to be called with the correct URL (which the
  // renderer only knows about asynchronously).
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
  LogToConsole(frame, mojom::blink::ConsoleMessageLevel::kInfo,
               String("A soft navigation has been detected: ") + url_);
  TRACE_EVENT_INSTANT("scheduler",
                      "SoftNavigationHeuristics_SoftNavigationDetected");
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
