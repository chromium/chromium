// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "base/logging.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

namespace blink {

// static
const char SoftNavigationHeuristics::kSupplementName[] =
    "SoftNavigationHeuristics";

SoftNavigationHeuristics* SoftNavigationHeuristics::From(
    LocalDOMWindow& window) {
  SoftNavigationHeuristics* heuristics =
      Supplement<LocalDOMWindow>::From<SoftNavigationHeuristics>(window);
  if (!heuristics) {
    heuristics = MakeGarbageCollected<SoftNavigationHeuristics>(window);
    ProvideTo(window, heuristics);
  }
  return heuristics;
}

void SoftNavigationHeuristics::ResetHeuristic() {
  // Reset previously seen indicators and task IDs.
  flag_set_.Clear();
  potential_soft_navigation_task_ids_.clear();
}

void SoftNavigationHeuristics::UserInitiatedClick(ScriptState* script_state) {
  // Set task ID to the current one.
  ThreadScheduler* scheduler = ThreadScheduler::Current();
  DCHECK(scheduler);
  // This should not be called off-main-thread.
  DCHECK(scheduler->GetTaskAttributionTracker());
  ResetHeuristic();
  scheduler->GetTaskAttributionTracker()->RegisterObserver(this);
}

bool SoftNavigationHeuristics::IsCurrentTaskDescendantOfClickEventHandler(
    ScriptState* script_state) {
  if (potential_soft_navigation_task_ids_.IsEmpty()) {
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
void SoftNavigationHeuristics::ClickEventEnded(ScriptState* script_state,
                                               bool is_cancelled) {
  if (is_cancelled) {
    flag_set_.Put(FlagType::kEventCancelled);
    CheckSoftNavigation(script_state);
  }
  ThreadScheduler* scheduler = ThreadScheduler::Current();
  DCHECK(scheduler);
  scheduler->GetTaskAttributionTracker()->UnregisterObserver();
}

bool SoftNavigationHeuristics::SetFlagIfDescendantAndCheck(
    ScriptState* script_state,
    FlagType type) {
  if (!IsCurrentTaskDescendantOfClickEventHandler(script_state)) {
    // A non-descendent URL change should not set the flag.
    return false;
  }
  flag_set_.Put(type);
  CheckSoftNavigation(script_state);
  return true;
}

void SoftNavigationHeuristics::SawURLChange(ScriptState* script_state) {
  if (!SetFlagIfDescendantAndCheck(script_state, FlagType::kURLChange)) {
    ResetHeuristic();
  }
}

void SoftNavigationHeuristics::ModifiedDOM(ScriptState* script_state) {
  SetFlagIfDescendantAndCheck(script_state, FlagType::kDOMModification);
}

void SoftNavigationHeuristics::CheckSoftNavigation(ScriptState* script_state) {
  if (flag_set_ == FlagTypeSet::All()) {
    ScriptState::Scope scope(script_state);
    if (LocalFrame* frame =
            ToLocalFrameIfNotDetached(script_state->GetContext())) {
      LocalDOMWindow* window = frame->DomWindow();
      if (window && frame->IsMainFrame()) {
        ++soft_navigation_count_;
        ResetHeuristic();
        if (RuntimeEnabledFeatures::SoftNavigationHeuristicsLoggingEnabled()) {
          auto* console_message = MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kJavaScript,
              mojom::blink::ConsoleMessageLevel::kInfo,
              String("A soft navigation has been detected."));
          window->AddConsoleMessage(console_message);
          // TODO(yoav): trace event as well.
        }
      }
    }
  }
}

void SoftNavigationHeuristics::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
}

void SoftNavigationHeuristics::OnCreateTaskScope(
    const scheduler::TaskId& task_id) {
  // We're inside a click event handler, so need to add this task to the set of
  // potential soft navigation root tasks.
  potential_soft_navigation_task_ids_.insert(task_id.value());
}

}  // namespace blink
