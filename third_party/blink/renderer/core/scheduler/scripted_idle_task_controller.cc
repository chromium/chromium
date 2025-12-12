// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/scripted_idle_task_controller.h"

#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using DecrementOnDelete = blink::ScriptedIdleTaskController::DecrementOnDelete;

namespace base {

// Cancellation traits for a "scheduler idle task".
template <>
struct CallbackCancellationTraits<
    blink::ScriptedIdleTaskController::SchedulerIdleTaskDeclType,
    std::tuple<blink::WeakPersistent<blink::ScriptedIdleTaskController>,
               blink::ScriptedIdleTaskController::CallbackId,
               DecrementOnDelete>> {
  static constexpr bool is_cancellable = true;

  static bool IsCancelled(
      blink::ScriptedIdleTaskController::SchedulerIdleTaskDeclType,
      const blink::WeakPersistent<blink::ScriptedIdleTaskController>&
          controller,
      const blink::ScriptedIdleTaskController::CallbackId& id,
      const DecrementOnDelete&) {
    if (!controller) {
      return true;
    }
    controller->OnCheckSchedulerIdleTaskIsCancelled();
    return !controller->HasCallback(id);
  }

  static bool MaybeValid(
      blink::ScriptedIdleTaskController::SchedulerIdleTaskDeclType,
      const blink::WeakPersistent<blink::ScriptedIdleTaskController>&
          controller,
      const blink::ScriptedIdleTaskController::CallbackId&,
      const DecrementOnDelete&) {
    // No effort is made return a thread-safe guess of validity.
    return true;
  }
};

}  // namespace base

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

}  // namespace

BASE_FEATURE(kRemoveCancelledScriptedIdleTasks,
             base::FEATURE_ENABLED_BY_DEFAULT);

IdleTask::~IdleTask() {
  CHECK(!delayed_task_handle_.IsValid());
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
  CHECK(idle_tasks_.empty());
}

void ScriptedIdleTaskController::Trace(Visitor* visitor) const {
  visitor->Trace(idle_tasks_);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
  Supplement<ExecutionContext>::Trace(visitor);
}

int ScriptedIdleTaskController::NextCallbackId() {
  CHECK(!IsValidCallbackId(0));
  CHECK(!IsValidCallbackId(-1));

  while (true) {
    ++next_callback_id_;

    if (!IsValidCallbackId(next_callback_id_)) {
      CHECK_EQ(next_callback_id_, -1, base::NotFatalUntil::M138);
      next_callback_id_wrapped_around_ = true;
      next_callback_id_ = 1;
    }

    if (!idle_tasks_.Contains(next_callback_id_)) {
      return next_callback_id_;
    }
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

ScriptedIdleTaskController::DecrementOnDelete::DecrementOnDelete(
    RefCountedCounter counter)
    : counter_(std::move(counter)) {}

ScriptedIdleTaskController::DecrementOnDelete::~DecrementOnDelete() {
  if (counter_) {
    DecrementNow();
  }
}

ScriptedIdleTaskController::DecrementOnDelete::DecrementOnDelete(
    DecrementOnDelete&&) = default;
ScriptedIdleTaskController::DecrementOnDelete&
ScriptedIdleTaskController::DecrementOnDelete::operator=(DecrementOnDelete&&) =
    default;

void ScriptedIdleTaskController::DecrementOnDelete::DecrementNow() {
  CHECK(counter_, base::NotFatalUntil::M136);
  CHECK_GT(counter_->data, 0u, base::NotFatalUntil::M136);
  --counter_->data;
  counter_.reset();
}

void ScriptedIdleTaskController::PostSchedulerIdleAndTimeoutTasks(
    CallbackId id,
    uint32_t timeout_millis) {
  auto it = idle_tasks_.find(id);
  CHECK_NE(it, idle_tasks_.end());
  CHECK(!it->value->delayed_task_handle_.IsValid());

  // Note: be careful about memory usage of this method.
  // 1. In certain corner case scenarios, millions of callbacks per minute could
  //    be processed. The memory usage per callback should be minimized as much
  //    as possible.
  // 2. `timeout_millis` is page-originated and doesn't have any reasonable
  //    limit. When a callback is processed, it's critical to remove the timeout
  //    task from the queue. Failure to do so is likely to result in OOM.
  if (timeout_millis > 0) {
    auto callback = BindOnce(&ScriptedIdleTaskController::SchedulerTimeoutTask,
                             WrapWeakPersistent(this), id);
    it->value->delayed_task_handle_ =
        GetExecutionContext()
            ->GetTaskRunner(TaskType::kIdleTask)
            ->PostCancelableDelayedTask(base::subtle::PostDelayedTaskPassKey(),
                                        FROM_HERE, std::move(callback),
                                        base::Milliseconds(timeout_millis));
  }

  PostSchedulerIdleTask(it);
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

  // The delta between `IdleTask`s and "scheduler idle tasks" increased.
  CleanupSchedulerIdleTasks();
}

bool ScriptedIdleTaskController::HasCallback(CallbackId id) const {
  return idle_tasks_.Contains(id);
}

void ScriptedIdleTaskController::PostSchedulerIdleTask(
    IdleTaskMap::iterator it) {
  // Track that there is a "scheduler idle task" queued for the IdleTask.
  CHECK(!it->value->has_scheduler_idle_task_, base::NotFatalUntil::M138);
  it->value->has_scheduler_idle_task_ = true;

  // Track the number of outstanding "scheduler idle tasks".
  ++num_scheduler_idle_tasks_->data;

  // Post the scheduler idle task.
  scheduler_->PostIdleTask(
      FROM_HERE, blink::BindOnce(&ScriptedIdleTaskController::SchedulerIdleTask,
                                 WrapWeakPersistent(this), it->key,
                                 DecrementOnDelete(num_scheduler_idle_tasks_)));
}

void ScriptedIdleTaskController::SchedulerIdleTask(
    CallbackId id,
    DecrementOnDelete decrement_on_delete,
    base::TimeTicks deadline) {
  CHECK_GT(num_scheduler_idle_tasks_->data, 0u, base::NotFatalUntil::M136);

  // Consume `decrement_on_delete` before running the task, to maintain the
  // invariant that `num_scheduler_idle_tasks_` <= `idle_tasks_.size()` after
  // invoking `RemoveCancelledIdleTasks()` on the scheduler, even if the idle
  // task cancels itself (without this, an idle task that cancels itself would
  // be counted in `num_scheduler_idle_tasks_` while running, but not in
  // `idle_tasks_.size()`.
  decrement_on_delete.DecrementNow();

  auto it = idle_tasks_.find(id);
  if (it == idle_tasks_.end()) {
    return;
  }

  // Track that there is no more "scheduler idle task" queued for this IdleTask.
  CHECK(it->value->has_scheduler_idle_task_, base::NotFatalUntil::M138);
  it->value->has_scheduler_idle_task_ = false;

  if (paused_) {
    // Reschedule when unpaused.
    idle_tasks_to_reschedule_.emplace_back(id);
    return;
  }

  // If we are going to yield immediately, reschedule the callback for later.
  if (ThreadScheduler::Current()->ShouldYieldForHighPriorityWork()) {
    PostSchedulerIdleTask(it);
    return;
  }

  // This probe needs to be called only when the Idle task is standalone, as in
  // doesn't come from the FrameScheduler, to make sure it goes through
  // performance monitoring channels.
  probe::FrameRelatedTask idle_task(GetExecutionContext());
  RunIdleTask(id, deadline, IdleDeadline::CallbackType::kCalledWhenIdle);
}

void ScriptedIdleTaskController::SchedulerTimeoutTask(CallbackId id) {
  // The timeout task is cancelled when the IdleTask is removed from
  // `idle_tasks_`, so this should only run if the task is in `idle_tasks_`.
  CHECK(idle_tasks_.Contains(id), base::NotFatalUntil::M138);
  if (!idle_tasks_.Contains(id)) {
    return;
  }

  // This task uses `blink::TaskType::kIdleTask` which has freezable and
  // pauseable `blink::scheduler::MainThreadTaskQueue::QueueTraits`, so it
  // shouldn't be scheduled while paused.
  CHECK(!paused_);

  RunIdleTask(id, /*deadline=*/base::TimeTicks::Now(),
              IdleDeadline::CallbackType::kCalledByTimeout);

  // The delta between `IdleTask`s and "scheduler idle tasks" increased when
  // RunIdleTask() above removed the `IdleTask` from `idle_tasks_`.
  CleanupSchedulerIdleTasks();
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
  CHECK_NE(idle_task_iter, idle_tasks_.end());
  IdleTask* idle_task = idle_task_iter->value;
  DCHECK(idle_task);

  base::TimeDelta allotted_time =
      std::max(deadline - base::TimeTicks::Now(), base::TimeDelta());

  probe::AsyncTask async_task(GetExecutionContext(),
                              idle_task->async_task_context());
  probe::UserCallback probe(GetExecutionContext(), "requestIdleCallback",
                            AtomicString(), true);

  bool cross_origin_isolated_capability =
      GetExecutionContext() &&
      GetExecutionContext()->CrossOriginIsolatedCapability();
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

  // The delta between `IdleTask`s and "scheduler idle tasks" increased.
  CleanupSchedulerIdleTasks();
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

void ScriptedIdleTaskController::OnCheckSchedulerIdleTaskIsCancelled() {
  ++num_is_cancelled_checks_;
}

void ScriptedIdleTaskController::CleanupSchedulerIdleTasks() {
  if (num_scheduler_idle_tasks_->data < idle_tasks_.size() ||
      num_scheduler_idle_tasks_->data - idle_tasks_.size() <= 1000 ||
      !base::FeatureList::IsEnabled(kRemoveCancelledScriptedIdleTasks)) {
    return;
  }

  const uint64_t num_scheduler_idle_tasks_before =
      num_scheduler_idle_tasks_->data;
  const uint64_t num_is_cancelled_checks_before = num_is_cancelled_checks_;

  scheduler_->RemoveCancelledIdleTasks();

  // TODO(crbug.com/394266102): Remove after the bug is understood and fixed.
  const uint64_t num_scheduler_idle_tasks_after =
      num_scheduler_idle_tasks_->data;
  const uint64_t num_is_cancelled_checks_after = num_is_cancelled_checks_;
  const size_t num_idle_tasks = idle_tasks_.size();
  base::debug::Alias(&num_scheduler_idle_tasks_before);
  base::debug::Alias(&num_is_cancelled_checks_before);
  base::debug::Alias(&num_scheduler_idle_tasks_after);
  base::debug::Alias(&num_is_cancelled_checks_after);
  base::debug::Alias(&num_idle_tasks);

  // IsCancelled() should be called exactly once per "scheduler idle task".
  // TODO(crbug.com/394266102): Remove after the bug is understood and fixed.
  CHECK_EQ(num_is_cancelled_checks_ - num_is_cancelled_checks_before,
           num_scheduler_idle_tasks_before, base::NotFatalUntil::M138);

  // There should be at most one "scheduler idle task" per IdleTask.
  // Note: When tasks are in `idle_tasks_to_reschedule_`, it is possible to
  // have less "scheduler idle tasks" than IdleTasks.
  CHECK_LE(num_scheduler_idle_tasks_->data, idle_tasks_.size(),
           base::NotFatalUntil::M138)
      << " Wrapped: " << next_callback_id_wrapped_around_;
}

void ScriptedIdleTaskController::ContextPaused() {
  paused_ = true;
}

void ScriptedIdleTaskController::ContextUnpaused() {
  DCHECK(paused_);
  paused_ = false;

  // Reschedule `IdleTask`s for which `SchedulerIdleTask` ran while paused.
  for (auto& id : idle_tasks_to_reschedule_) {
    auto it = idle_tasks_.find(id);
    if (it == idle_tasks_.end()) {
      continue;
    }
    PostSchedulerIdleTask(it);
  }
  idle_tasks_to_reschedule_.clear();
}

}  // namespace blink
