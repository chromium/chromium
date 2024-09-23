// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/scripted_idle_task_controller.h"

#include "base/debug/crash_logging.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
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

namespace {

void UpdateMaxIdleTasksCrashKey(size_t num_pending_idle_tasks) {
  // A crash key with the highest number of pending `IdleTasks` in a single
  // `ScriptedIdleTaskController` instance, rounded down to the nearest hundred
  // to minimize the frequency of updates and reduce overhead.
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "max_idle_tasks", base::debug::CrashKeySize::Size32);
  static std::optional<size_t> crash_key_value;

  const size_t num_pending_idle_tasks_rounded_down =
      (num_pending_idle_tasks / 100) * 100;
  if (!crash_key_value.has_value() ||
      crash_key_value.value() < num_pending_idle_tasks_rounded_down) {
    base::debug::SetCrashKeyString(
        crash_key, base::NumberToString(num_pending_idle_tasks_rounded_down));
    crash_key_value = num_pending_idle_tasks_rounded_down;
  }
}

void UpdateMaxSchedulerIdleTasksCrashKey(
    size_t num_pending_scheduler_idle_tasks) {
  // A crash key with the highest number of scheduler idle tasks outstanding for
  // a single `ScriptedIdleTaskController` instance, rounded down to the nearest
  // hundred to minimize the frequency of updates and reduce overhead.
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "max_scheduler_idle_tasks", base::debug::CrashKeySize::Size32);
  static std::optional<size_t> crash_key_value;

  const size_t num_pending_scheduler_idle_tasks_rounded_down =
      (num_pending_scheduler_idle_tasks / 100) * 100;
  if (!crash_key_value.has_value() ||
      crash_key_value.value() < num_pending_scheduler_idle_tasks_rounded_down) {
    base::debug::SetCrashKeyString(
        crash_key,
        base::NumberToString(num_pending_scheduler_idle_tasks_rounded_down));
    crash_key_value = num_pending_scheduler_idle_tasks_rounded_down;
  }
}

}  // namespace

BASE_FEATURE(kScriptedIdleTaskControllerOOMFix,
             "ScriptedIdleTaskControllerOOMFix",
             base::FEATURE_DISABLED_BY_DEFAULT);

IdleTask::~IdleTask() {
  CHECK(!delayed_task_handle_.IsValid());
}

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

const char ScriptedIdleTaskController::kSupplementName[] =
    "ScriptedIdleTaskController";

// static
ScriptedIdleTaskController& ScriptedIdleTaskController::From(
    ExecutionContext& context) {
  ScriptedIdleTaskController* controller =
      Supplement<ExecutionContext>::From<ScriptedIdleTaskController>(&context);
  if (!controller) {
    controller = MakeGarbageCollected<ScriptedIdleTaskController>(&context);
    Supplement<ExecutionContext>::ProvideTo(context, controller);
  }
  return *controller;
}

ScriptedIdleTaskController::ScriptedIdleTaskController(
    ExecutionContext* context)
    : ExecutionContextLifecycleStateObserver(context),
      Supplement<ExecutionContext>(*context),
      scheduler_(ThreadScheduler::Current()) {
  UpdateStateIfNeeded();
}

ScriptedIdleTaskController::~ScriptedIdleTaskController() {
  CHECK(idle_tasks_.empty(), base::NotFatalUntil::M135);
}

void ScriptedIdleTaskController::Trace(Visitor* visitor) const {
  visitor->Trace(idle_tasks_);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
  Supplement<ExecutionContext>::Trace(visitor);
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

void ScriptedIdleTaskController::Dispose() {
  RemoveAllIdleTasks();
}

ScriptedIdleTaskController::CallbackId
ScriptedIdleTaskController::RegisterCallback(
    IdleTask* idle_task,
    const IdleRequestOptions* options) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    return 0;
  }

  DCHECK(idle_task);
  CallbackId id = NextCallbackId();
  idle_tasks_.Set(id, idle_task);
  UpdateMaxIdleTasksCrashKey(idle_tasks_.size());
  uint32_t timeout_millis = options->timeout();

  idle_task->async_task_context()->Schedule(GetExecutionContext(),
                                            "requestIdleCallback");

  PostSchedulerIdleAndTimeoutTasks(id, timeout_millis);
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "RequestIdleCallback", inspector_idle_callback_request_event::Data,
      GetExecutionContext(), id, timeout_millis);
  return id;
}

void ScriptedIdleTaskController::PostSchedulerIdleAndTimeoutTasks(
    CallbackId id,
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
    auto callback =
        WTF::BindOnce(&ScriptedIdleTaskController::SchedulerTimeoutTask,
                      WrapWeakPersistent(this), id);
    delayed_task_handle =
        GetExecutionContext()
            ->GetTaskRunner(TaskType::kIdleTask)
            ->PostCancelableDelayedTask(base::subtle::PostDelayedTaskPassKey(),
                                        FROM_HERE, std::move(callback),
                                        base::Milliseconds(timeout_millis));

    if (base::FeatureList::IsEnabled(kScriptedIdleTaskControllerOOMFix)) {
      auto it = idle_tasks_.find(id);
      CHECK_NE(it, idle_tasks_.end());
      CHECK(!it->value->delayed_task_handle_.IsValid());
      it->value->delayed_task_handle_ = std::move(delayed_task_handle);
    }
  }

  PostSchedulerIdleTask(id,
                        DelayedTaskCanceler(std::move(delayed_task_handle)));
}

void ScriptedIdleTaskController::CancelCallback(CallbackId id) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    return;
  }

  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "CancelIdleCallback", inspector_idle_callback_cancel_event::Data,
      GetExecutionContext(), id);
  if (!IsValidCallbackId(id)) {
    return;
  }

  RemoveIdleTask(id);
}

void ScriptedIdleTaskController::PostSchedulerIdleTask(
    CallbackId id,
    DelayedTaskCanceler canceler) {
  ++num_pending_scheduler_idle_tasks_;
  UpdateMaxSchedulerIdleTasksCrashKey(num_pending_scheduler_idle_tasks_);

  scheduler_->PostIdleTask(
      FROM_HERE,
      WTF::BindOnce(&ScriptedIdleTaskController::SchedulerIdleTask,
                    WrapWeakPersistent(this), id, std::move(canceler)));
}

void ScriptedIdleTaskController::SchedulerIdleTask(
    CallbackId id,
    ScriptedIdleTaskController::DelayedTaskCanceler /* canceler */,
    base::TimeTicks deadline) {
  CHECK_GT(num_pending_scheduler_idle_tasks_, 0u, base::NotFatalUntil::M135);
  --num_pending_scheduler_idle_tasks_;

  if (!idle_tasks_.Contains(id)) {
    return;
  }

  if (paused_) {
    if (base::FeatureList::IsEnabled(kScriptedIdleTaskControllerOOMFix)) {
      // Reschedule when unpaused.
      idle_tasks_to_reschedule_.emplace_back(id);
    } else {
      // All `IdleTask`s are rescheduled when unpaused.
    }
    return;
  }

  // If we are going to yield immediately, reschedule the callback for later.
  if (ThreadScheduler::Current()->ShouldYieldForHighPriorityWork()) {
    // Note: `canceler` is implicitly deleted in this code path, which means
    // that the timeout will not be honored when the
    // "ScriptedIdleTaskControllerOOMFix" feature is disabled (when the feature
    // is enabled, the `DelayedTaskHandle` is stored on the `IdleTask`).
    PostSchedulerIdleTask(id, DelayedTaskCanceler());
    return;
  }

  RunIdleTask(id, deadline, IdleDeadline::CallbackType::kCalledWhenIdle);
}

void ScriptedIdleTaskController::SchedulerTimeoutTask(CallbackId id) {
  if (!idle_tasks_.Contains(id)) {
    return;
  }

  // This task uses `blink::TaskType::kIdleTask` which has freezable and
  // pauseable `blink::scheduler::MainThreadTaskQueue::QueueTraits`, so it
  // shouldn't be scheduled while paused.
  CHECK(!paused_, base::NotFatalUntil::M133);

  // TODO(crbug.com/365114039): Remove this in M133 if the above CHECK holds.
  if (paused_) {
    // Reschedule when unpaused.
    idle_tasks_with_expired_timeout_.push_back(id);
    return;
  }

  RunIdleTask(id, /*deadline=*/base::TimeTicks::Now(),
              IdleDeadline::CallbackType::kCalledByTimeout);
}

void ScriptedIdleTaskController::RunIdleTask(
    CallbackId id,
    base::TimeTicks deadline,
    IdleDeadline::CallbackType callback_type) {
  DCHECK(!paused_);

  // Keep the idle task in |idle_tasks_| so that it's still wrapper-traced.
  // TODO(https://crbug.com/796145): Remove this hack once on-stack objects
  // get supported by either of wrapper-tracing or unified GC.
  auto idle_task_iter = idle_tasks_.find(id);
  CHECK_NE(idle_task_iter, idle_tasks_.end(), base::NotFatalUntil::M133);
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
  RemoveIdleTask(id);
}

void ScriptedIdleTaskController::RemoveIdleTask(CallbackId id) {
  auto it = idle_tasks_.find(id);
  if (it == idle_tasks_.end()) {
    return;
  }
  // A `base::DelayedTaskHandle` must be explicitly canceled before deletion.
  it->value->delayed_task_handle_.CancelTask();
  idle_tasks_.erase(it);
}

void ScriptedIdleTaskController::RemoveAllIdleTasks() {
  for (auto& idle_task : idle_tasks_) {
    // A `base::DelayedTaskHandle` must be explicitly canceled before deletion.
    idle_task.value->delayed_task_handle_.CancelTask();
  }
  idle_tasks_.clear();
}

void ScriptedIdleTaskController::ContextDestroyed() {
  RemoveAllIdleTasks();
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

  // Reschedule `IdleTask`s for which `SchedulerTimeoutTask` ran while paused.
  for (auto& id : idle_tasks_with_expired_timeout_) {
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kIdleTask)
        ->PostTask(
            FROM_HERE,
            WTF::BindOnce(&ScriptedIdleTaskController::SchedulerTimeoutTask,
                          WrapWeakPersistent(this), id));
  }
  idle_tasks_with_expired_timeout_.clear();

  if (base::FeatureList::IsEnabled(kScriptedIdleTaskControllerOOMFix)) {
    // Reschedule `IdleTask`s for which `SchedulerIdleTask` ran while paused.
    for (auto& idle_task : idle_tasks_to_reschedule_) {
      PostSchedulerIdleTask(idle_task, DelayedTaskCanceler());
    }
    idle_tasks_to_reschedule_.clear();
  } else {
    // Reschedule all `IdleTask`s.
    for (auto& idle_task : idle_tasks_) {
      PostSchedulerIdleTask(idle_task.key, DelayedTaskCanceler());
    }
  }
}

}  // namespace blink
