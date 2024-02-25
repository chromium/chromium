// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"

#include <cstdint>
#include <optional>

#include "base/check_op.h"
#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_pump.h"
#include "base/task/common/lazy_now.h"
#include "base/time/tick_clock.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {
namespace scheduler {

using base::LazyNow;
using base::sequence_manager::TaskQueue;

TaskQueueThrottler::TaskQueueThrottler(
    base::sequence_manager::TaskQueue* task_queue,
    const base::TickClock* tick_clock)
    : task_queue_(task_queue), tick_clock_(tick_clock) {}

TaskQueueThrottler::~TaskQueueThrottler() {
  if (IsThrottled())
    DisableThrottling();

  for (BudgetPool* budget_pool : budget_pools_) {
    budget_pool->UnregisterThrottler(this);
  }
}

void TaskQueueThrottler::IncreaseThrottleRefCount() {
  if (throttling_ref_count_++ != 0)
    return;

  // Task queue is newly throttled.
  TRACE_EVENT1("renderer.scheduler", "TaskQueueThrottler_TaskQueueThrottled",
               "task_queue", static_cast<void*>(task_queue_));

  task_queue_->SetThrottler(this);
  if (!task_queue_->IsQueueEnabled())
    return;

  UpdateQueueState(tick_clock_->NowTicks());
}

void TaskQueueThrottler::DecreaseThrottleRefCount() {
  DCHECK_GT(throttling_ref_count_, 0U);
  if (--throttling_ref_count_ != 0)
    return;

  TRACE_EVENT1("renderer.scheduler", "TaskQueueThrottler_TaskQueueUnthrottled",
               "task_queue", static_cast<void*>(task_queue_));
  DisableThrottling();
}

bool TaskQueueThrottler::IsThrottled() const {
  return throttling_ref_count_ > 0;
}

std::optional<base::sequence_manager::WakeUp>
TaskQueueThrottler::GetNextAllowedWakeUpImpl(
    LazyNow* lazy_now,
    std::optional<base::sequence_manager::WakeUp> next_wake_up,
    bool has_ready_task) {
  DCHECK(IsThrottled());
  DCHECK(task_queue_->IsQueueEnabled());

  if (has_ready_task) {
    base::TimeTicks allowed_run_time = GetNextAllowedRunTime(lazy_now->Now());
    // If |allowed_run_time| is null, immediate tasks can run immediately and
    // they don't require a delayed wake up (a delayed wake up might be required
    // for delayed tasks, see below). Otherwise, schedule a delayed wake up to
    // update the fence in the future.
    if (!allowed_run_time.is_null()) {
      // WakeUpResolution::kLow and DelayPolicy::kFlexibleNoSooner are always
      // used for throttled tasks since those tasks can tolerate having their
      // execution being delayed.
      return base::sequence_manager::WakeUp{
          allowed_run_time, base::MessagePump::GetLeewayForCurrentThread(),
          base::sequence_manager::WakeUpResolution::kLow,
          base::subtle::DelayPolicy::kFlexibleNoSooner};
    }
  }
  if (!next_wake_up.has_value())
    return std::nullopt;

  base::TimeTicks desired_run_time =
      std::max(next_wake_up->time, lazy_now->Now());
  base::TimeTicks allowed_run_time = GetNextAllowedRunTime(desired_run_time);
  if (allowed_run_time.is_null())
    allowed_run_time = desired_run_time;

  // Throttled tasks can tolerate having their execution being delayed, so
  // transform "precise" delay policy into "flexible no sooner".
  return base::sequence_manager::WakeUp{
      allowed_run_time, next_wake_up->leeway,
      base::sequence_manager::WakeUpResolution::kLow,
      next_wake_up->delay_policy == base::subtle::DelayPolicy::kPrecise
          ? base::subtle::DelayPolicy::kFlexibleNoSooner
          : next_wake_up->delay_policy};
}

void TaskQueueThrottler::OnHasImmediateTask() {
  DCHECK(IsThrottled());
  DCHECK(task_queue_->IsQueueEnabled());

  TRACE_EVENT0("renderer.scheduler", "TaskQueueThrottler::OnHasImmediateTask");

  LazyNow lazy_now(tick_clock_);
  if (CanRunTasksAt(lazy_now.Now())) {
    UpdateFence(lazy_now.Now());
  } else {
    task_queue_->UpdateWakeUp(&lazy_now);
  }
}

std::optional<base::sequence_manager::WakeUp>
TaskQueueThrottler::GetNextAllowedWakeUp(
    LazyNow* lazy_now,
    std::optional<base::sequence_manager::WakeUp> next_desired_wake_up,
    bool has_ready_task) {
  TRACE_EVENT0("renderer.scheduler", "TaskQueueThrottler::OnNextWakeUpChanged");

  return GetNextAllowedWakeUpImpl(lazy_now, next_desired_wake_up,
                                  has_ready_task);
}

void TaskQueueThrottler::OnTaskRunTimeReported(base::TimeTicks start_time,
                                               base::TimeTicks end_time) {
  if (!IsThrottled())
    return;

  for (BudgetPool* budget_pool : budget_pools_) {
    budget_pool->RecordTaskRunTime(start_time, end_time);
  }
}

void TaskQueueThrottler::UpdateQueueState(base::TimeTicks now) {
  if (!task_queue_->IsQueueEnabled() || !IsThrottled())
    return;
  LazyNow lazy_now(now);
  if (CanRunTasksAt(now)) {
    UpdateFence(now);
  } else {
    // Insert a fence of an appropriate type.
    std::optional<QueueBlockType> block_type = GetBlockType(now);
    DCHECK(block_type);
    switch (block_type.value()) {
      case QueueBlockType::kAllTasks:
        task_queue_->InsertFence(
            TaskQueue::InsertFencePosition::kBeginningOfTime);
        break;
      case QueueBlockType::kNewTasksOnly:
        if (!task_queue_->HasActiveFence()) {
          // Insert a new non-fully blocking fence only when there is no fence
          // already in order avoid undesired unblocking of old tasks.
          task_queue_->InsertFence(TaskQueue::InsertFencePosition::kNow);
        }
        break;
    }
    TRACE_EVENT_INSTANT("renderer.scheduler",
                        "TaskQueueThrottler::InsertFence");
  }
  task_queue_->UpdateWakeUp(&lazy_now);
}

void TaskQueueThrottler::OnWakeUp(base::LazyNow* lazy_now) {
  DCHECK(IsThrottled());
  for (BudgetPool* budget_pool : budget_pools_)
    budget_pool->OnWakeUp(lazy_now->Now());

  base::TimeTicks now = lazy_now->Now();
  DCHECK(CanRunTasksAt(now));
  UpdateFence(now);
}

void TaskQueueThrottler::UpdateFence(base::TimeTicks now) {
  DCHECK(IsThrottled());
  // Unblock queue if we can run tasks immediately.
  base::TimeTicks unblock_until = GetTimeTasksCanRunUntil(now);
  if (unblock_until.is_max()) {
    task_queue_->RemoveFence();
  } else if (unblock_until > now) {
    task_queue_->InsertFenceAt(unblock_until);
  } else {
    DCHECK_EQ(unblock_until, now);
    task_queue_->InsertFence(TaskQueue::InsertFencePosition::kNow);
  }
}

void TaskQueueThrottler::DisableThrottling() {
  task_queue_->RemoveFence();
  task_queue_->ResetThrottler();
}

std::optional<QueueBlockType> TaskQueueThrottler::GetBlockType(
    base::TimeTicks now) const {
  bool has_new_tasks_only_block = false;

  for (BudgetPool* budget_pool : budget_pools_) {
    if (!budget_pool->CanRunTasksAt(now)) {
      if (budget_pool->GetBlockType() == QueueBlockType::kAllTasks)
        return QueueBlockType::kAllTasks;
      DCHECK_EQ(budget_pool->GetBlockType(), QueueBlockType::kNewTasksOnly);
      has_new_tasks_only_block = true;
    }
  }

  if (has_new_tasks_only_block)
    return QueueBlockType::kNewTasksOnly;
  return std::nullopt;
}

void TaskQueueThrottler::AddBudgetPool(BudgetPool* budget_pool) {
  budget_pools_.insert(budget_pool);
}

void TaskQueueThrottler::RemoveBudgetPool(BudgetPool* budget_pool) {
  budget_pools_.erase(budget_pool);
}

bool TaskQueueThrottler::CanRunTasksAt(base::TimeTicks moment) {
  for (BudgetPool* budget_pool : budget_pools_) {
    if (!budget_pool->CanRunTasksAt(moment)) {
      return false;
    }
  }

  return true;
}

base::TimeTicks TaskQueueThrottler::GetNextAllowedRunTime(
    base::TimeTicks desired_runtime) const {
  // If |desired_runtime| isn't affected by any BudgetPool, TimeTicks() is
  // returned.
  base::TimeTicks result = base::TimeTicks();

  for (BudgetPool* budget_pool : budget_pools_) {
    if (budget_pool->CanRunTasksAt(desired_runtime))
      continue;
    result =
        std::max(result, budget_pool->GetNextAllowedRunTime(desired_runtime));
  }
  return result;
}

base::TimeTicks TaskQueueThrottler::GetTimeTasksCanRunUntil(
    base::TimeTicks now) const {
  // Start with no known limit for the time tasks can run until.
  base::TimeTicks result = base::TimeTicks::Max();

  for (BudgetPool* budget_pool : budget_pools_) {
    result = std::min(result, budget_pool->GetTimeTasksCanRunUntil(now));
  }

  return result;
}

void TaskQueueThrottler::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("throttling_ref_count", throttling_ref_count_);
}

}  // namespace scheduler
}  // namespace blink
