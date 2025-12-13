// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/window_idle_tasks.h"

#include <optional>

#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/scheduler/dom_scheduler.h"
#include "third_party/blink/renderer/core/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/core/scheduler/scheduler_task_context.h"
#include "third_party/blink/renderer/core/scheduler/scripted_idle_task_controller.h"
#include "third_party/blink/renderer/core/scheduler/task_attribution_util.h"
#include "third_party/blink/renderer/core/scheduler/web_scheduling_task_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"

namespace blink {

namespace {

// `V8IdleTask` is the adapter class for the conversion from
// `V8IdleRequestCallback` to `IdleTask`.
class V8IdleTask : public IdleTask {
 public:
  static V8IdleTask* Create(V8IdleRequestCallback* callback,
                            ExecutionContext* scheduling_context) {
    return MakeGarbageCollected<V8IdleTask>(callback, scheduling_context);
  }

  V8IdleTask(V8IdleRequestCallback* callback,
             ExecutionContext* scheduling_context)
      : callback_(callback) {
    ScriptState* script_state = callback_->CallbackRelevantScriptState();
    auto* signal =
        DOMScheduler::scheduler(*scheduling_context)
            ->GetFixedPriorityTaskSignal(
                script_state, WebSchedulingPriority::kBackgroundPriority);
    web_scheduling_task_state_ = MakeGarbageCollected<WebSchedulingTaskState>(
        CaptureCurrentTaskStateIfMainWorld(script_state),
        MakeGarbageCollected<SchedulerTaskContext>(
            scheduling_context, /*abort_source=*/nullptr, signal));
  }

  ~V8IdleTask() override = default;

  void invoke(IdleDeadline* deadline) override {
    std::optional<scheduler::TaskAttributionTracker::TaskScope>
        task_attribution_scope;
    if (auto* tracker =
            scheduler::TaskAttributionTracker::From(callback_->GetIsolate())) {
      task_attribution_scope = tracker->SetCurrentTaskState(
          web_scheduling_task_state_, TaskScopeType::kRequestIdleCallback);
    }
    callback_->InvokeAndReportException(nullptr, deadline);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(callback_);
    visitor->Trace(web_scheduling_task_state_);
    IdleTask::Trace(visitor);
  }

 private:
  Member<V8IdleRequestCallback> callback_;
  Member<WebSchedulingTaskState> web_scheduling_task_state_;
};

}  // namespace

int WindowIdleTasks::requestIdleCallback(LocalDOMWindow& window,
                                         V8IdleRequestCallback* callback,
                                         const IdleRequestOptions* options) {
  return ScriptedIdleTaskController::From(window).RegisterCallback(
      V8IdleTask::Create(callback, &window), options);
}

void WindowIdleTasks::cancelIdleCallback(LocalDOMWindow& window, int id) {
  ScriptedIdleTaskController::From(window).CancelCallback(id);
}

}  // namespace blink
