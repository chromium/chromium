// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scripted_idle_task_controller.h"

#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/idle_request_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

namespace internal {

class IdleRequestCallbackWrapper
    : public RefCounted<IdleRequestCallbackWrapper> {
 public:
  static scoped_refptr<IdleRequestCallbackWrapper> Create(
      ScriptedIdleTaskController::CallbackId id,
      ScriptedIdleTaskController* controller) {
    return base::AdoptRef(new IdleRequestCallbackWrapper(id, controller));
  }
  virtual ~IdleRequestCallbackWrapper() = default;

  static void IdleTaskFired(
      scoped_refptr<IdleRequestCallbackWrapper> callback_wrapper,
      base::TimeTicks deadline) {
    if (ScriptedIdleTaskController* controller =
            callback_wrapper->Controller()) {
      // If we are going to yield immediately, reschedule the callback for
      // later.
      if (ThreadScheduler::Current()->ShouldYieldForHighPriorityWork()) {
        controller->ScheduleCallback(std::move(callback_wrapper),
                                     /* timeout_millis */ 0);
        return;
      }
      controller->CallbackFired(callback_wrapper->Id(), deadline,
                                IdleDeadline::CallbackType::kCalledWhenIdle);
    }
    callback_wrapper->Cancel();
  }

  static void TimeoutFired(
      scoped_refptr<IdleRequestCallbackWrapper> callback_wrapper) {
    if (ScriptedIdleTaskController* controller =
            callback_wrapper->Controller()) {
      controller->CallbackFired(callback_wrapper->Id(), base::TimeTicks::Now(),
                                IdleDeadline::CallbackType::kCalledByTimeout);
    }
    callback_wrapper->Cancel();
  }

  void Cancel() { controller_ = nullptr; }

  ScriptedIdleTaskController::CallbackId Id() const { return id_; }
  ScriptedIdleTaskController* Controller() const { return controller_; }

 private:
  IdleRequestCallbackWrapper(ScriptedIdleTaskController::CallbackId id,
                             ScriptedIdleTaskController* controller)
      : id_(id), controller_(controller) {}

  ScriptedIdleTaskController::CallbackId id_;
  WeakPersistent<ScriptedIdleTaskController> controller_;
};

}  // namespace internal

ScriptedIdleTaskController::V8IdleTask::V8IdleTask(
    V8IdleRequestCallback* callback)
    : callback_(callback) {}

void ScriptedIdleTaskController::V8IdleTask::Trace(Visitor* visitor) {
  visitor->Trace(callback_);
  ScriptedIdleTaskController::IdleTask::Trace(visitor);
}

void ScriptedIdleTaskController::V8IdleTask::invoke(IdleDeadline* deadline) {
  callback_->InvokeAndReportException(nullptr, deadline);
}

ScriptedIdleTaskController::ScriptedIdleTaskController(
    ExecutionContext* context)
    : ContextLifecycleStateObserver(context),
      scheduler_(ThreadScheduler::Current()),
      next_callback_id_(0),
      paused_(false) {}

ScriptedIdleTaskController::~ScriptedIdleTaskController() = default;

void ScriptedIdleTaskController::Trace(Visitor* visitor) {
  visitor->Trace(idle_tasks_);
  ContextLifecycleStateObserver::Trace(visitor);
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
  base::TimeTicks queue_timestamp = base::TimeTicks::Now();
  uint32_t timeout_millis = options->timeout();
  idle_tasks_.Set(id, MakeGarbageCollected<QueuedIdleTask>(
                          idle_task, queue_timestamp, timeout_millis));

  probe::AsyncTaskScheduled(GetExecutionContext(), "requestIdleCallback",
                            idle_task->async_task_id());

  scoped_refptr<internal::IdleRequestCallbackWrapper> callback_wrapper =
      internal::IdleRequestCallbackWrapper::Create(id, this);
  ScheduleCallback(std::move(callback_wrapper), timeout_millis);
  TRACE_EVENT_INSTANT1("devtools.timeline", "RequestIdleCallback",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_idle_callback_request_event::Data(
                           GetExecutionContext(), id, timeout_millis));
  return id;
}

void ScriptedIdleTaskController::ScheduleCallback(
    scoped_refptr<internal::IdleRequestCallbackWrapper> callback_wrapper,
    uint32_t timeout_millis) {
  scheduler_->PostIdleTask(
      FROM_HERE, WTF::Bind(&internal::IdleRequestCallbackWrapper::IdleTaskFired,
                           callback_wrapper));
  if (timeout_millis > 0) {
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kIdleTask)
        ->PostDelayedTask(
            FROM_HERE,
            WTF::Bind(&internal::IdleRequestCallbackWrapper::TimeoutFired,
                      callback_wrapper),
            base::TimeDelta::FromMilliseconds(timeout_millis));
  }
}

void ScriptedIdleTaskController::CancelCallback(CallbackId id) {
  TRACE_EVENT_INSTANT1(
      "devtools.timeline", "CancelIdleCallback", TRACE_EVENT_SCOPE_THREAD,
      "data",
      inspector_idle_callback_cancel_event::Data(GetExecutionContext(), id));
  if (!IsValidCallbackId(id))
    return;

  idle_tasks_.erase(id);
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
  QueuedIdleTask* queued_idle_task = idle_task_iter->value;
  DCHECK(queued_idle_task);
  IdleTask* idle_task = queued_idle_task->task();
  DCHECK(idle_task);

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta allotted_time = std::max(deadline - now, base::TimeDelta());

  probe::AsyncTask async_task(GetExecutionContext(),
                              idle_task->async_task_id());
  probe::UserCallback probe(GetExecutionContext(), "requestIdleCallback",
                            AtomicString(), true);

  TRACE_EVENT1(
      "devtools.timeline", "FireIdleCallback", "data",
      inspector_idle_callback_fire_event::Data(
          GetExecutionContext(), id, allotted_time.InMillisecondsF(),
          callback_type == IdleDeadline::CallbackType::kCalledByTimeout));
  idle_task->invoke(
      MakeGarbageCollected<IdleDeadline>(deadline, callback_type));

  RecordIdleTaskMetrics(queued_idle_task, now, callback_type);

  // Finally there is no need to keep the idle task alive.
  //
  // Do not use the iterator because the idle task might update |idle_tasks_|.
  idle_tasks_.erase(id);
}

void ScriptedIdleTaskController::ContextDestroyed(ExecutionContext*) {
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

  // Run any pending timeouts.
  Vector<CallbackId> pending_timeouts;
  pending_timeouts_.swap(pending_timeouts);
  for (auto& id : pending_timeouts)
    RunCallback(id, base::TimeTicks::Now(),
                IdleDeadline::CallbackType::kCalledByTimeout);

  // Repost idle tasks for any remaining callbacks.
  for (auto& idle_task : idle_tasks_) {
    scoped_refptr<internal::IdleRequestCallbackWrapper> callback_wrapper =
        internal::IdleRequestCallbackWrapper::Create(idle_task.key, this);
    scheduler_->PostIdleTask(
        FROM_HERE,
        WTF::Bind(&internal::IdleRequestCallbackWrapper::IdleTaskFired,
                  callback_wrapper));
  }
}

void ScriptedIdleTaskController::RecordIdleTaskMetrics(
    QueuedIdleTask* queued_idle_task,
    base::TimeTicks run_timestamp,
    IdleDeadline::CallbackType callback_type) {
  UMA_HISTOGRAM_ENUMERATION(
      "WebCore.ScriptedIdleTaskController.IdleTaskCallbackType", callback_type);
  UMA_HISTOGRAM_COUNTS_100000(
      "WebCore.ScriptedIdleTaskController.IdleTaskTimeout",
      queued_idle_task->timeout_millis());
  if (callback_type == IdleDeadline::CallbackType::kCalledWhenIdle) {
    DCHECK_GE(run_timestamp, queued_idle_task->queue_timestamp());
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "WebCore.ScriptedIdleTaskController.IdleTaskQueueingTime",
        run_timestamp - queued_idle_task->queue_timestamp());
  }
  if (callback_type == IdleDeadline::CallbackType::kCalledByTimeout) {
    UMA_HISTOGRAM_COUNTS_100000(
        "WebCore.ScriptedIdleTaskController.IdleTaskTimeoutExceeded",
        queued_idle_task->timeout_millis());
  }
}

ScriptedIdleTaskController::QueuedIdleTask::QueuedIdleTask(
    IdleTask* idle_task,
    base::TimeTicks queue_timestamp,
    uint32_t timeout_millis)
    : task_(idle_task),
      queue_timestamp_(queue_timestamp),
      timeout_millis_(timeout_millis) {}

void ScriptedIdleTaskController::QueuedIdleTask::Trace(Visitor* visitor) {
  visitor->Trace(task_);
}

}  // namespace blink
