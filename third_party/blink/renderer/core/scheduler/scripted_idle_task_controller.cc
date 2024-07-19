// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/scripted_idle_task_controller.h"

#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
namespace blink {

ScriptedIdleTaskController::DelayedTaskCanceler::DelayedTaskCanceler() =
    default;
ScriptedIdleTaskController::DelayedTaskCanceler::DelayedTaskCanceler(
    base::DelayedTaskHandle delayed_task_handle)
    : delayed_task_handle_(std::move(delayed_task_handle)) {}
ScriptedIdleTaskController::DelayedTaskCanceler::DelayedTaskCanceler(
    DelayedTaskCanceler&&) = default;
ScriptedIdleTaskController::DelayedTaskCanceler&
ScriptedIdleTaskController::DelayedTaskCanceler::operator=(
    ScriptedIdleTaskController::DelayedTaskCanceler&&) = default;

ScriptedIdleTaskController::DelayedTaskCanceler::~DelayedTaskCanceler() {
  delayed_task_handle_.CancelTask();
}

ScriptedIdleTaskController::ScriptedIdleTaskController(
    ExecutionContext* context)
    : ExecutionContextLifecycleStateObserver(context),
      scheduler_(ThreadScheduler::Current()),
      next_callback_id_(0),
      paused_(false) {}

ScriptedIdleTaskController::~ScriptedIdleTaskController() = default;

void ScriptedIdleTaskController::Trace(Visitor* visitor) const {
  visitor->Trace(idle_tasks_);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

int ScriptedIdleTaskController::NextCallbackId() {
  while (true) {
    ++next_callback_id_;

    if (!IsValidCallbackId(next_callback_id_))
      next_callback_id_ = 1;

    if (!idle_tasks_.Contains(next_callback_id_))
      return next_callback_id_;
  }
}

ScriptedIdleTaskController::CallbackId
ScriptedIdleTaskController::RegisterCallback(
    IdleTask* idle_task,
    const IdleRequestOptions* options) {
  DCHECK(idle_task);

  CallbackId id = NextCallbackId();
  idle_tasks_.Set(id, idle_task);
  uint32_t timeout_millis = options->timeout();

  idle_task->async_task_context()->Schedule(GetExecutionContext(),
                                            "requestIdleCallback");

  ScheduleCallback(id, timeout_millis);
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "RequestIdleCallback", inspector_idle_callback_request_event::Data,
      GetExecutionContext(), id, timeout_millis);
  return id;
}

void ScriptedIdleTaskController::ScheduleCallback(CallbackId id,
                                                  uint32_t timeout_millis) {
  // Note: be careful about memory usage of this method.
  // 1. In certain corner case scenarios, millions of callbacks per minute could
  //    be processed. The memory usage per callback should be minimized as much
  //    as possible.
  // 2. `timeout_millis` is page-originated and doesn't have any reasonable
  //    limit. When a callback is processed, it's critical to remove the timeout
  //    task from the queue. Failure to do so is likely to result in OOM.
  base::DelayedTaskHandle delayed_task_handle;
  if (timeout_millis > 0) {
    auto callback = WTF::BindOnce(&ScriptedIdleTaskController::TimeoutFired,
                                  WrapWeakPersistent(this), id);
    delayed_task_handle =
        GetExecutionContext()
            ->GetTaskRunner(TaskType::kIdleTask)
            ->PostCancelableDelayedTask(base::subtle::PostDelayedTaskPassKey(),
                                        FROM_HERE, std::move(callback),
                                        base::Milliseconds(timeout_millis));
  }

  scheduler_->PostIdleTask(
      FROM_HERE,
      WTF::BindOnce(&ScriptedIdleTaskController::IdleTaskFired,
                    WrapWeakPersistent(this), id,
                    DelayedTaskCanceler(std::move(delayed_task_handle))));
}

void ScriptedIdleTaskController::CancelCallback(CallbackId id) {
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "CancelIdleCallback", inspector_idle_callback_cancel_event::Data,
      GetExecutionContext(), id);
  if (!IsValidCallbackId(id)) {
    return;
  }

  idle_tasks_.erase(id);
}

void ScriptedIdleTaskController::IdleTaskFired(
    CallbackId id,
    ScriptedIdleTaskController::DelayedTaskCanceler /* canceler */,
    base::TimeTicks deadline) {
  // If we are going to yield immediately, reschedule the callback for
  // later.
  if (ThreadScheduler::Current()->ShouldYieldForHighPriorityWork()) {
    ScheduleCallback(id, /* timeout_millis */ 0);
    return;
  }
  CallbackFired(id, deadline, IdleDeadline::CallbackType::kCalledWhenIdle);
}

void ScriptedIdleTaskController::TimeoutFired(CallbackId id) {
  CallbackFired(id, base::TimeTicks::Now(),
                IdleDeadline::CallbackType::kCalledByTimeout);
}

void ScriptedIdleTaskController::CallbackFired(
    CallbackId id,
    base::TimeTicks deadline,
    IdleDeadline::CallbackType callback_type) {
  if (!idle_tasks_.Contains(id))
    return;

  if (paused_) {
    if (callback_type == IdleDeadline::CallbackType::kCalledByTimeout) {
      // Queue for execution when we are resumed.
      pending_timeouts_.push_back(id);
    }
    // Just drop callbacks called while suspended, these will be reposted on the
    // idle task queue when we are resumed.
    return;
  }

  RunCallback(id, deadline, callback_type);
}

void ScriptedIdleTaskController::RunCallback(
    CallbackId id,
    base::TimeTicks deadline,
    IdleDeadline::CallbackType callback_type) {
  DCHECK(!paused_);

  // Keep the idle task in |idle_tasks_| so that it's still wrapper-traced.
  // TODO(https://crbug.com/796145): Remove this hack once on-stack objects
  // get supported by either of wrapper-tracing or unified GC.
  auto idle_task_iter = idle_tasks_.find(id);
  if (idle_task_iter == idle_tasks_.end())
    return;
  IdleTask* idle_task = idle_task_iter->value;
  DCHECK(idle_task);

  base::TimeDelta allotted_time =
      std::max(deadline - base::TimeTicks::Now(), base::TimeDelta());

  probe::AsyncTask async_task(GetExecutionContext(),
                              idle_task->async_task_context());
  probe::UserCallback probe(GetExecutionContext(), "requestIdleCallback",
                            AtomicString(), true);

  bool cross_origin_isolated_capability =
      GetExecutionContext()
          ? GetExecutionContext()->CrossOriginIsolatedCapability()
          : false;
  DEVTOOLS_TIMELINE_TRACE_EVENT(
      "FireIdleCallback", inspector_idle_callback_fire_event::Data,
      GetExecutionContext(), id, allotted_time.InMillisecondsF(),
      callback_type == IdleDeadline::CallbackType::kCalledByTimeout);
  idle_task->invoke(MakeGarbageCollected<IdleDeadline>(
      deadline, cross_origin_isolated_capability, callback_type));

  // Finally there is no need to keep the idle task alive.
  //
  // Do not use the iterator because the idle task might update |idle_tasks_|.
  idle_tasks_.erase(id);
}

void ScriptedIdleTaskController::ContextDestroyed() {
  idle_tasks_.clear();
}

void ScriptedIdleTaskController::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {
  if (state != mojom::FrameLifecycleState::kRunning)
    ContextPaused();
  else
    ContextUnpaused();
}

void ScriptedIdleTaskController::ContextPaused() {
  paused_ = true;
}

void ScriptedIdleTaskController::ContextUnpaused() {
  DCHECK(paused_);
  paused_ = false;

  // Run any pending timeouts as separate tasks, since it's not allowed to
  // execute script from lifecycle callbacks.
  for (auto& id : pending_timeouts_) {
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kIdleTask)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&ScriptedIdleTaskController::TimeoutFired,
                                 WrapWeakPersistent(this), id));
  }
  pending_timeouts_.clear();

  // Repost idle tasks for any remaining callbacks.
  for (auto& idle_task : idle_tasks_) {
    scheduler_->PostIdleTask(
        FROM_HERE, WTF::BindOnce(&ScriptedIdleTaskController::IdleTaskFired,
                                 WrapWeakPersistent(this), idle_task.key,
                                 DelayedTaskCanceler()));
  }
}

}  // namespace blink
