// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"

#include <cstdint>

#include "base/debug/stack_trace.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/time/tick_clock.h"
#include "third_party/blink/renderer/platform/scheduler/common/thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/throttled_time_domain.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::LazyNow;
using base::sequence_manager::TaskQueue;

namespace {

base::Optional<base::TimeTicks> NextTaskRunTime(LazyNow* lazy_now,
                                                TaskQueue* queue) {
  if (queue->HasTaskToRunImmediately())
    return lazy_now->Now();
  return queue->GetNextScheduledWakeUp();
}

template <class T>
T Min(const base::Optional<T>& optional, const T& value) {
  if (!optional) {
    return value;
  }
  return std::min(optional.value(), value);
}

template <class T>
base::Optional<T> Min(const base::Optional<T>& a, const base::Optional<T>& b) {
  if (!b)
    return a;
  if (!a)
    return b;
  return std::min(a.value(), b.value());
}

template <class T>
T Max(const base::Optional<T>& optional, const T& value) {
  if (!optional)
    return value;
  return std::max(optional.value(), value);
}

template <class T>
base::Optional<T> Max(const base::Optional<T>& a, const base::Optional<T>& b) {
  if (!b)
    return a;
  if (!a)
    return b;
  return std::max(a.value(), b.value());
}

}  // namespace

TaskQueueThrottler::TaskQueueThrottler(
    ThreadSchedulerImpl* thread_scheduler,
    TraceableVariableController* tracing_controller)
    : control_task_runner_(thread_scheduler->ControlTaskRunner()),
      thread_scheduler_(thread_scheduler),
      tracing_controller_(tracing_controller),
      tick_clock_(thread_scheduler->GetTickClock()),
      time_domain_(new ThrottledTimeDomain()),
      allow_throttling_(true) {
  pump_throttled_tasks_closure_.Reset(base::BindRepeating(
      &TaskQueueThrottler::PumpThrottledTasks, weak_factory_.GetWeakPtr()));
  forward_immediate_work_callback_ =
      base::BindRepeating(&TaskQueueThrottler::OnQueueNextWakeUpChanged,
                          weak_factory_.GetWeakPtr());

  thread_scheduler_->RegisterTimeDomain(time_domain_.get());
}

TaskQueueThrottler::~TaskQueueThrottler() {
  // It's possible for queues to be still throttled, so we need to tidy up
  // before unregistering the time domain.
  for (const TaskQueueMap::value_type& map_entry : queue_details_) {
    TaskQueue* task_queue = map_entry.key;
    if (IsThrottled(task_queue)) {
      task_queue->SetTimeDomain(thread_scheduler_->GetActiveTimeDomain());
      task_queue->RemoveFence();
    }
  }

  thread_scheduler_->UnregisterTimeDomain(time_domain_.get());
}

void TaskQueueThrottler::IncreaseThrottleRefCount(TaskQueue* task_queue) {
  auto insert_result = queue_details_.insert(
      task_queue, std::make_unique<Metadata>(task_queue, this));
  if (!insert_result.stored_value->value->IncrementRefCount())
    return;

  // Task queue is newly throttled.
  TRACE_EVENT1("renderer.scheduler", "TaskQueueThrottler_TaskQueueThrottled",
               "task_queue", task_queue);

  if (!allow_throttling_)
    return;

  task_queue->SetTimeDomain(time_domain_.get());
  // This blocks any tasks from |task_queue| until PumpThrottledTasks() to
  // enforce task alignment.
  task_queue->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);

  if (!task_queue->IsQueueEnabled())
    return;

  if (!task_queue->IsEmpty()) {
    LazyNow lazy_now(tick_clock_);
    OnQueueNextWakeUpChanged(task_queue,
                             NextTaskRunTime(&lazy_now, task_queue).value());
  }
}

void TaskQueueThrottler::DecreaseThrottleRefCount(TaskQueue* task_queue) {
  TaskQueueMap::iterator iter = queue_details_.find(task_queue);

  if (iter == queue_details_.end())
    return;
  if (!iter->value->DecrementRefCount())
    return;

  TRACE_EVENT1("renderer.scheduler", "TaskQueueThrottler_TaskQueueUnthrottled",
               "task_queue", task_queue);

  MaybeDeleteQueueMetadata(iter);

  if (!allow_throttling_)
    return;

  task_queue->SetTimeDomain(thread_scheduler_->GetActiveTimeDomain());
  task_queue->RemoveFence();
}

bool TaskQueueThrottler::IsThrottled(TaskQueue* task_queue) const {
  if (!allow_throttling_)
    return false;

  auto find_it = queue_details_.find(task_queue);
  if (find_it == queue_details_.end())
    return false;
  return find_it->value->throttling_ref_count() > 0;
}

void TaskQueueThrottler::ShutdownTaskQueue(TaskQueue* task_queue) {
  auto find_it = queue_details_.find(task_queue);
  if (find_it == queue_details_.end())
    return;

  // Reset a time domain reference to a valid domain, otherwise it's possible
  // to get a stale reference when deleting queue.
  task_queue->SetTimeDomain(thread_scheduler_->GetActiveTimeDomain());
  task_queue->RemoveFence();

  // Copy intended.
  auto budget_pools = find_it->value->budget_pools();
  for (BudgetPool* budget_pool : budget_pools) {
    budget_pool->UnregisterQueue(task_queue);
  }

  // Iterator may have been deleted by BudgetPool::RemoveQueue, so don't
  // use it here.
  queue_details_.erase(task_queue);

  // NOTE: Observer is automatically unregistered when unregistering task queue.
}

void TaskQueueThrottler::OnQueueNextWakeUpChanged(
    TaskQueue* queue,
    base::TimeTicks next_wake_up) {
  if (!control_task_runner_->RunsTasksInCurrentSequence()) {
    control_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(forward_immediate_work_callback_,
                                  base::RetainedRef(queue), next_wake_up));
    return;
  }

  TRACE_EVENT0("renderer.scheduler",
               "TaskQueueThrottler::OnQueueNextWakeUpChanged");

  // We don't expect this to get called for disabled queues, but we can't DCHECK
  // because of the above thread hop.  Just bail out if the queue is disabled.
  if (!queue->IsQueueEnabled())
    return;

  base::TimeTicks now = tick_clock_->NowTicks();
  next_wake_up = std::max(now, next_wake_up);

  auto find_it = queue_details_.find(queue);
  if (find_it == queue_details_.end())
    return;

  for (BudgetPool* budget_pool : find_it->value->budget_pools()) {
    budget_pool->OnQueueNextWakeUpChanged(queue, now, next_wake_up);
  }

  // TODO(altimin): This probably can be removed —- budget pools should
  // schedule this.
  base::TimeTicks next_allowed_run_time =
      GetNextAllowedRunTime(queue, next_wake_up);
  MaybeSchedulePumpThrottledTasks(
      FROM_HERE, now, std::max(next_wake_up, next_allowed_run_time));
}

void TaskQueueThrottler::PumpThrottledTasks() {
  TRACE_EVENT0("renderer.scheduler", "TaskQueueThrottler::PumpThrottledTasks");
  pending_pump_throttled_tasks_runtime_.reset();

  LazyNow lazy_now(tick_clock_);

  for (const auto& pair : budget_pools_)
    pair.key->OnWakeUp(lazy_now.Now());

  for (const TaskQueueMap::value_type& map_entry : queue_details_) {
    TaskQueue* task_queue = map_entry.key;
    UpdateQueueSchedulingLifecycleStateInternal(lazy_now.Now(), task_queue,
                                                true);
  }
}

/* static */
base::TimeTicks TaskQueueThrottler::AlignedThrottledRunTime(
    base::TimeTicks unthrottled_runtime) {
  const base::TimeDelta one_second = base::TimeDelta::FromSeconds(1);
  return unthrottled_runtime + one_second -
         ((unthrottled_runtime - base::TimeTicks()) % one_second);
}

void TaskQueueThrottler::MaybeSchedulePumpThrottledTasks(
    const base::Location& from_here,
    base::TimeTicks now,
    base::TimeTicks unaligned_runtime) {
  if (!allow_throttling_)
    return;

  // TODO(altimin): Consider removing alignment here.
  base::TimeTicks runtime =
      std::max(now, unaligned_runtime)
          .SnappedToNextTick(base::TimeTicks(),
                             base::TimeDelta::FromSeconds(1));
  DCHECK_LE(now, runtime);

  // If there is a pending call to PumpThrottledTasks and it's sooner than
  // |runtime| then return.
  if (pending_pump_throttled_tasks_runtime_ &&
      runtime >= pending_pump_throttled_tasks_runtime_.value()) {
    return;
  }

  pending_pump_throttled_tasks_runtime_ = runtime;

  pump_throttled_tasks_closure_.Cancel();

  base::TimeDelta delay = pending_pump_throttled_tasks_runtime_.value() - now;
  TRACE_EVENT1("renderer.scheduler",
               "TaskQueueThrottler::MaybeSchedulePumpThrottledTasks",
               "delay_till_next_pump_ms", delay.InMilliseconds());
  control_task_runner_->PostDelayedTask(
      from_here, pump_throttled_tasks_closure_.GetCallback(), delay);
}

CPUTimeBudgetPool* TaskQueueThrottler::CreateCPUTimeBudgetPool(
    const char* name) {
  CPUTimeBudgetPool* time_budget_pool = new CPUTimeBudgetPool(
      name, this, tracing_controller_, tick_clock_->NowTicks());
  budget_pools_.Set(time_budget_pool, base::WrapUnique(time_budget_pool));
  return time_budget_pool;
}

WakeUpBudgetPool* TaskQueueThrottler::CreateWakeUpBudgetPool(const char* name) {
  WakeUpBudgetPool* wake_up_budget_pool =
      new WakeUpBudgetPool(name, this, tick_clock_->NowTicks());
  budget_pools_.Set(wake_up_budget_pool, base::WrapUnique(wake_up_budget_pool));
  return wake_up_budget_pool;
}

void TaskQueueThrottler::OnTaskRunTimeReported(TaskQueue* task_queue,
                                               base::TimeTicks start_time,
                                               base::TimeTicks end_time) {
  if (!IsThrottled(task_queue))
    return;

  auto find_it = queue_details_.find(task_queue);
  if (find_it == queue_details_.end())
    return;

  for (BudgetPool* budget_pool : find_it->value->budget_pools()) {
    budget_pool->RecordTaskRunTime(task_queue, start_time, end_time);
  }
}

void TaskQueueThrottler::UpdateQueueSchedulingLifecycleState(
    base::TimeTicks now,
    TaskQueue* queue) {
  UpdateQueueSchedulingLifecycleStateInternal(now, queue, false);
}

void TaskQueueThrottler::UpdateQueueSchedulingLifecycleStateInternal(
    base::TimeTicks now,
    TaskQueue* queue,
    bool is_wake_up) {
  if (!queue->IsQueueEnabled() || !IsThrottled(queue)) {
    return;
  }

  LazyNow lazy_now(now);

  base::Optional<base::TimeTicks> next_desired_run_time =
      NextTaskRunTime(&lazy_now, queue);

  if (CanRunTasksAt(queue, now, is_wake_up)) {
    // Unblock queue if we can run tasks immediately.
    base::Optional<base::TimeTicks> unblock_until =
        GetTimeTasksCanRunUntil(queue, now, is_wake_up);
    DCHECK(unblock_until);
    if (unblock_until.value() > now) {
      queue->InsertFenceAt(unblock_until.value());
    } else if (unblock_until.value() == now) {
      queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
    } else {
      DCHECK_GE(unblock_until.value(), now);
    }

    // Throttled time domain does not schedule wake-ups without explicitly
    // being told so.
    if (next_desired_run_time && next_desired_run_time.value() != now &&
        next_desired_run_time.value() < unblock_until) {
      time_domain_->SetNextTaskRunTime(next_desired_run_time.value());
    }

    base::Optional<base::TimeTicks> next_wake_up =
        queue->GetNextScheduledWakeUp();
    // TODO(altimin, crbug.com/813218): Find a testcase to repro freezes
    // mentioned in the bug.
    if (next_wake_up) {
      MaybeSchedulePumpThrottledTasks(
          FROM_HERE, now, GetNextAllowedRunTime(queue, next_wake_up.value()));
    }

    return;
  }

  if (!next_desired_run_time)
    return;

  base::TimeTicks next_run_time =
      GetNextAllowedRunTime(queue, next_desired_run_time.value());

  // Insert a fence of an approriate type.
  base::Optional<QueueBlockType> block_type = GetQueueBlockType(now, queue);
  DCHECK(block_type);

  switch (block_type.value()) {
    case QueueBlockType::kAllTasks:
      queue->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);

      {
        // Braces limit the scope for a declared variable. Does not compile
        // otherwise.
        TRACE_EVENT1(
            "renderer.scheduler",
            "TaskQueueThrottler::PumpThrottledTasks_ExpensiveTaskThrottled",
            "throttle_time_in_seconds",
            (next_run_time - next_desired_run_time.value()).InSecondsF());
      }
      break;
    case QueueBlockType::kNewTasksOnly:
      if (!queue->HasActiveFence()) {
        // Insert a new non-fully blocking fence only when there is no fence
        // already in order avoid undesired unblocking of old tasks.
        queue->InsertFence(TaskQueue::InsertFencePosition::kNow);
      }
      break;
  }

  // Schedule a pump.
  MaybeSchedulePumpThrottledTasks(FROM_HERE, now, next_run_time);
}

base::Optional<QueueBlockType> TaskQueueThrottler::GetQueueBlockType(
    base::TimeTicks now,
    TaskQueue* queue) {
  auto find_it = queue_details_.find(queue);
  if (find_it == queue_details_.end())
    return base::nullopt;

  bool has_new_tasks_only_block = false;

  for (BudgetPool* budget_pool : find_it->value->budget_pools()) {
    if (!budget_pool->CanRunTasksAt(now, false)) {
      if (budget_pool->GetBlockType() == QueueBlockType::kAllTasks)
        return QueueBlockType::kAllTasks;
      DCHECK_EQ(budget_pool->GetBlockType(), QueueBlockType::kNewTasksOnly);
      has_new_tasks_only_block = true;
    }
  }

  if (has_new_tasks_only_block)
    return QueueBlockType::kNewTasksOnly;
  return base::nullopt;
}

void TaskQueueThrottler::AsValueInto(base::trace_event::TracedValue* state,
                                     base::TimeTicks now) const {
  if (pending_pump_throttled_tasks_runtime_) {
    state->SetDouble(
        "next_throttled_tasks_pump_in_seconds",
        (pending_pump_throttled_tasks_runtime_.value() - now).InSecondsF());
  }

  state->SetBoolean("allow_throttling", allow_throttling_);

  state->BeginDictionary("time_budget_pools");
  for (const auto& map_entry : budget_pools_) {
    BudgetPool* pool = map_entry.key;
    pool->AsValueInto(state, now);
  }
  state->EndDictionary();

  state->BeginDictionary("queue_details");
  for (const auto& map_entry : queue_details_) {
    state->BeginDictionaryWithCopiedName(PointerToString(map_entry.key));
    state->SetInteger(
        "throttling_ref_count",
        static_cast<int>(map_entry.value->throttling_ref_count()));
    state->EndDictionary();
  }
  state->EndDictionary();
}

void TaskQueueThrottler::AddQueueToBudgetPool(TaskQueue* queue,
                                              BudgetPool* budget_pool) {
  auto insert_result =
      queue_details_.insert(queue, std::make_unique<Metadata>(queue, this));

  Metadata* metadata = insert_result.stored_value->value.get();

  DCHECK(metadata->budget_pools().find(budget_pool) ==
         metadata->budget_pools().end());

  metadata->budget_pools().insert(budget_pool);
}

void TaskQueueThrottler::RemoveQueueFromBudgetPool(TaskQueue* queue,
                                                   BudgetPool* budget_pool) {
  auto find_it = queue_details_.find(queue);
  DCHECK(find_it != queue_details_.end() &&
         find_it->value->budget_pools().find(budget_pool) !=
             find_it->value->budget_pools().end());

  find_it->value->budget_pools().erase(budget_pool);

  MaybeDeleteQueueMetadata(find_it);
}

void TaskQueueThrottler::UnregisterBudgetPool(BudgetPool* budget_pool) {
  budget_pools_.erase(budget_pool);
}

base::TimeTicks TaskQueueThrottler::GetNextAllowedRunTime(
    TaskQueue* queue,
    base::TimeTicks desired_run_time) {
  base::TimeTicks next_run_time = desired_run_time;

  auto find_it = queue_details_.find(queue);
  if (find_it == queue_details_.end())
    return next_run_time;

  for (BudgetPool* budget_pool : find_it->value->budget_pools()) {
    next_run_time = std::max(
        next_run_time, budget_pool->GetNextAllowedRunTime(desired_run_time));
  }

  return next_run_time;
}

bool TaskQueueThrottler::CanRunTasksAt(TaskQueue* queue,
                                       base::TimeTicks moment,
                                       bool is_wake_up) {
  auto find_it = queue_details_.find(queue);
  if (find_it == queue_details_.end())
    return true;

  for (BudgetPool* budget_pool : find_it->value->budget_pools()) {
    if (!budget_pool->CanRunTasksAt(moment, is_wake_up))
      return false;
  }

  return true;
}

base::Optional<base::TimeTicks> TaskQueueThrottler::GetTimeTasksCanRunUntil(
    TaskQueue* queue,
    base::TimeTicks now,
    bool is_wake_up) const {
  base::Optional<base::TimeTicks> result;
  auto find_it = queue_details_.find(queue);
  if (find_it == queue_details_.end())
    return result;

  for (BudgetPool* budget_pool : find_it->value->budget_pools()) {
    result = Min(result, budget_pool->GetTimeTasksCanRunUntil(now, is_wake_up));
  }

  return result;
}

void TaskQueueThrottler::MaybeDeleteQueueMetadata(TaskQueueMap::iterator it) {
  if (it->value->throttling_ref_count() == 0 &&
      it->value->budget_pools().IsEmpty()) {
    queue_details_.erase(it);
  }
}

void TaskQueueThrottler::DisableThrottling() {
  if (!allow_throttling_)
    return;

  allow_throttling_ = false;

  for (const auto& map_entry : queue_details_) {
    if (map_entry.value->throttling_ref_count() == 0)
      continue;

    TaskQueue* queue = map_entry.key;

    queue->SetTimeDomain(thread_scheduler_->GetActiveTimeDomain());
    queue->RemoveFence();
  }

  pump_throttled_tasks_closure_.Cancel();
  pending_pump_throttled_tasks_runtime_ = base::nullopt;

  TRACE_EVENT0("renderer.scheduler", "TaskQueueThrottler_DisableThrottling");
}

void TaskQueueThrottler::EnableThrottling() {
  if (allow_throttling_)
    return;

  allow_throttling_ = true;

  LazyNow lazy_now(tick_clock_);

  for (const auto& map_entry : queue_details_) {
    if (map_entry.value->throttling_ref_count() == 0)
      continue;

    TaskQueue* queue = map_entry.key;

    // Throttling is enabled and task queue should be blocked immediately
    // to enforce task alignment.
    queue->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
    queue->SetTimeDomain(time_domain_.get());
    UpdateQueueSchedulingLifecycleState(lazy_now.Now(), queue);
  }

  TRACE_EVENT0("renderer.scheduler", "TaskQueueThrottler_EnableThrottling");
}

TaskQueueThrottler::Metadata::Metadata(base::sequence_manager::TaskQueue* queue,
                                       TaskQueueThrottler* throttler)
    : queue_(queue), throttler_(throttler) {}

TaskQueueThrottler::Metadata::~Metadata() {
  if (throttling_ref_count_ > 0)
    queue_->SetObserver(nullptr);
}

bool TaskQueueThrottler::Metadata::IncrementRefCount() {
  if (throttling_ref_count_++ == 0) {
    queue_->SetObserver(this);
    return true;
  }
  return false;
}

// Returns true if |throttling_ref_count_| is now zero.
bool TaskQueueThrottler::Metadata::DecrementRefCount() {
  if (throttling_ref_count_ == 0)
    return false;
  if (--throttling_ref_count_ == 0) {
    queue_->SetObserver(nullptr);
    return true;
  }
  return false;
}

void TaskQueueThrottler::Metadata::OnPostTask(base::Location from_here,
                                              base::TimeDelta delay) {}

void TaskQueueThrottler::Metadata::OnQueueNextWakeUpChanged(
    base::TimeTicks wake_up) {
  throttler_->OnQueueNextWakeUpChanged(queue_, wake_up);
}

}  // namespace scheduler
}  // namespace blink
