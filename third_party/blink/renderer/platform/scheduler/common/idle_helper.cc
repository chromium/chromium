// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/idle_helper.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/time_domain.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "third_party/blink/renderer/platform/scheduler/common/blink_scheduler_single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;

IdleHelper::IdleHelper(
    SchedulerHelper* helper,
    Delegate* delegate,
    const char* idle_period_tracing_name,
    base::TimeDelta required_quiescence_duration_before_long_idle_period,
    TaskQueue* idle_queue)
    : helper_(helper),
      delegate_(delegate),
      idle_queue_(idle_queue),
      idle_period_tracing_name_(idle_period_tracing_name),
      required_quiescence_duration_before_long_idle_period_(
          required_quiescence_duration_before_long_idle_period) {
  enable_next_long_idle_period_closure_.Reset(base::BindRepeating(
      &IdleHelper::EnableLongIdlePeriod, weak_factory_.GetWeakPtr()));
  on_idle_task_posted_closure_.Reset(base::BindRepeating(
      &IdleHelper::OnIdleTaskPostedOnMainThread, weak_factory_.GetWeakPtr()));
  idle_task_runner_ = base::MakeRefCounted<SingleThreadIdleTaskRunner>(
      base::MakeRefCounted<BlinkSchedulerSingleThreadTaskRunner>(
          idle_queue_->CreateTaskRunner(
              static_cast<int>(TaskType::kMainThreadTaskQueueIdle)),
          nullptr),
      helper_->ControlTaskRunner(), this);
  // This fence will block any idle tasks from running.
  idle_queue_->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
  idle_queue_->SetQueuePriority(TaskPriority::kBestEffortPriority);
}

IdleHelper::~IdleHelper() {
  Shutdown();
}

void IdleHelper::Shutdown() {
  if (is_shutdown_)
    return;

  EndIdlePeriod();
  is_shutdown_ = true;
  weak_factory_.InvalidateWeakPtrs();
}

void IdleHelper::RemoveCancelledIdleTasks() {
  idle_queue_->RemoveCancelledTasks();
}

IdleHelper::Delegate::Delegate() = default;

IdleHelper::Delegate::~Delegate() = default;

scoped_refptr<SingleThreadIdleTaskRunner> IdleHelper::IdleTaskRunner() {
  return idle_task_runner_;
}

IdleHelper::IdlePeriodState IdleHelper::ComputeNewLongIdlePeriodState(
    const base::TimeTicks now,
    base::TimeDelta* next_long_idle_period_delay_out) {
  helper_->CheckOnValidThread();

  if (!delegate_->CanEnterLongIdlePeriod(now,
                                         next_long_idle_period_delay_out)) {
    return IdlePeriodState::kNotInIdlePeriod;
  }

  auto wake_up = helper_->GetNextWakeUp();

  base::TimeDelta long_idle_period_duration;

  if (wake_up) {
    // Limit the idle period duration to be before the next pending task.
    long_idle_period_duration =
        std::min(wake_up->time - now, kMaximumIdlePeriodDuration);
  } else {
    long_idle_period_duration = kMaximumIdlePeriodDuration;
  }

  if (long_idle_period_duration >= kMinimumIdlePeriodDuration) {
    *next_long_idle_period_delay_out = long_idle_period_duration;
    if (!idle_queue_->HasTaskToRunImmediatelyOrReadyDelayedTask())
      return IdlePeriodState::kInLongIdlePeriodPaused;
    return IdlePeriodState::kInLongIdlePeriod;
  } else {
    // If we can't start the idle period yet then try again after wake-up.
    *next_long_idle_period_delay_out = kRetryEnableLongIdlePeriodDelay;
    return IdlePeriodState::kNotInIdlePeriod;
  }
}

bool IdleHelper::ShouldWaitForQuiescence() {
  helper_->CheckOnValidThread();

  if (required_quiescence_duration_before_long_idle_period_ ==
      base::TimeDelta()) {
    return false;
  }

  bool system_is_quiescent = helper_->GetAndClearSystemIsQuiescentBit();
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "ShouldWaitForQuiescence", "system_is_quiescent",
               system_is_quiescent);
  return !system_is_quiescent;
}

void IdleHelper::EnableLongIdlePeriod() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "EnableLongIdlePeriod");
  helper_->CheckOnValidThread();
  if (is_shutdown_)
    return;

  // End any previous idle period.
  EndIdlePeriod();

  if (ShouldWaitForQuiescence()) {
    helper_->ControlTaskRunner()->PostDelayedTask(
        FROM_HERE, enable_next_long_idle_period_closure_.GetCallback(),
        required_quiescence_duration_before_long_idle_period_);
    delegate_->IsNotQuiescent();
    return;
  }

  base::TimeTicks now(helper_->NowTicks());
  base::TimeDelta next_long_idle_period_delay;
  IdlePeriodState new_idle_period_state =
      ComputeNewLongIdlePeriodState(now, &next_long_idle_period_delay);
  if (new_idle_period_state != IdlePeriodState::kNotInIdlePeriod) {
    StartIdlePeriod(new_idle_period_state, now,
                    now + next_long_idle_period_delay);
  } else {
    // Otherwise wait for the next long idle period delay before trying again.
    helper_->ControlTaskRunner()->PostDelayedTask(
        FROM_HERE, enable_next_long_idle_period_closure_.GetCallback(),
        next_long_idle_period_delay);
  }
}

void IdleHelper::StartShortIdlePeriod(base::TimeTicks now,
                                      base::TimeTicks idle_period_deadline) {
  StartIdlePeriod(IdlePeriodState::kInShortIdlePeriod, now,
                  idle_period_deadline);
}

void IdleHelper::StartIdlePeriod(IdlePeriodState new_state,
                                 base::TimeTicks now,
                                 base::TimeTicks idle_period_deadline) {
  DCHECK(!is_shutdown_);
  DCHECK_GT(idle_period_deadline, now);
  helper_->CheckOnValidThread();
  DCHECK_NE(new_state, IdlePeriodState::kNotInIdlePeriod);

  // Allow any ready delayed idle tasks to run.
  idle_task_runner_->EnqueueReadyDelayedIdleTasks();

  base::TimeDelta idle_period_duration(idle_period_deadline - now);
  if (idle_period_duration < kMinimumIdlePeriodDuration) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "NotStartingIdlePeriodBecauseDeadlineIsTooClose",
                 "idle_period_duration_ms",
                 idle_period_duration.InMillisecondsF());
    return;
  }

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "StartIdlePeriod");
  if (!IsInIdlePeriod()) {
    helper_->AddTaskObserver(this);
  }

  // Use a fence to make sure any idle tasks posted after this point do not run
  // until the next idle period and unblock existing tasks.
  idle_queue_->InsertFence(TaskQueue::InsertFencePosition::kNow);

  SetIdlePeriodState(new_state, idle_period_deadline, now);
}

void IdleHelper::EndIdlePeriod() {
  if (is_shutdown_)
    return;

  helper_->CheckOnValidThread();
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "EndIdlePeriod");

  enable_next_long_idle_period_closure_.Cancel();
  on_idle_task_posted_closure_.Cancel();

  // If we weren't already within an idle period then early-out.
  if (!IsInIdlePeriod()) {
    return;
  }

  helper_->RemoveTaskObserver(this);

  // This fence will block any idle tasks from running.
  idle_queue_->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
  SetIdlePeriodState(IdlePeriodState::kNotInIdlePeriod, base::TimeTicks(),
                     base::TimeTicks());
}

void IdleHelper::WillProcessTask(const base::PendingTask& pending_task,
                                 bool was_blocked_or_low_priority) {
  DCHECK(!is_shutdown_);
}

void IdleHelper::DidProcessTask(const base::PendingTask& pending_task) {
  helper_->CheckOnValidThread();
  DCHECK(!is_shutdown_);
  DCHECK(IsInIdlePeriod());
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "DidProcessTask");
  if (idle_period_state_ != IdlePeriodState::kInLongIdlePeriodPaused &&
      helper_->NowTicks() >= idle_period_deadline_) {
    // If the idle period deadline has now been reached, either end the idle
    // period or trigger a new long-idle period.
    if (IsInLongIdlePeriod()) {
      EnableLongIdlePeriod();
    } else {
      DCHECK_EQ(idle_period_state_, IdlePeriodState::kInShortIdlePeriod);
      EndIdlePeriod();
    }
  }
}

void IdleHelper::UpdateLongIdlePeriodStateAfterIdleTask() {
  helper_->CheckOnValidThread();
  DCHECK(!is_shutdown_);
  DCHECK(IsInLongIdlePeriod());
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "UpdateLongIdlePeriodStateAfterIdleTask");

  if (!idle_queue_->HasTaskToRunImmediatelyOrReadyDelayedTask()) {
    // If there are no more idle tasks then pause long idle period ticks until a
    // new idle task is posted.
    SetIdlePeriodState(IdlePeriodState::kInLongIdlePeriodPaused,
                       idle_period_deadline_, base::TimeTicks());
  } else if (idle_queue_->BlockedByFence()) {
    // If there is still idle work to do then just start the next idle period.
    base::TimeDelta next_long_idle_period_delay;
    // Ensure that we kick the scheduler at the right time to
    // initiate the next idle period.
    next_long_idle_period_delay = std::max(
        base::TimeDelta(), idle_period_deadline_ - helper_->NowTicks());
    if (next_long_idle_period_delay.is_zero()) {
      EnableLongIdlePeriod();
    } else {
      helper_->ControlTaskRunner()->PostDelayedTask(
          FROM_HERE, enable_next_long_idle_period_closure_.GetCallback(),
          next_long_idle_period_delay);
    }
  }
}

base::TimeTicks IdleHelper::CurrentIdleTaskDeadlineForTesting() const {
  helper_->CheckOnValidThread();
  return idle_period_deadline_;
}

void IdleHelper::OnIdleTaskPosted() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "OnIdleTaskPosted");
  if (is_shutdown_)
    return;
  if (idle_task_runner_->RunsTasksInCurrentSequence()) {
    OnIdleTaskPostedOnMainThread();
  } else {
    helper_->ControlTaskRunner()->PostTask(
        FROM_HERE, on_idle_task_posted_closure_.GetCallback());
  }
}

void IdleHelper::OnIdleTaskPostedOnMainThread() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "OnIdleTaskPostedOnMainThread");
  if (is_shutdown_)
    return;
  delegate_->OnPendingTasksChanged(true);
  if (idle_period_state_ == IdlePeriodState::kInLongIdlePeriodPaused) {
    // Restart long idle period ticks.
    helper_->ControlTaskRunner()->PostTask(
        FROM_HERE, enable_next_long_idle_period_closure_.GetCallback());
  }
}

base::TimeTicks IdleHelper::WillProcessIdleTask() {
  helper_->CheckOnValidThread();
  DCHECK(!is_shutdown_);
  TraceIdleIdleTaskStart();
  return idle_period_deadline_;
}

void IdleHelper::DidProcessIdleTask() {
  helper_->CheckOnValidThread();
  if (is_shutdown_)
    return;
  TraceIdleIdleTaskEnd();
  if (IsInLongIdlePeriod()) {
    UpdateLongIdlePeriodStateAfterIdleTask();
  }
  delegate_->OnPendingTasksChanged(idle_queue_->GetNumberOfPendingTasks() > 0);
}

base::TimeTicks IdleHelper::NowTicks() {
  return helper_->NowTicks();
}

bool IdleHelper::IsInIdlePeriod() const {
  helper_->CheckOnValidThread();
  return idle_period_state_ != IdlePeriodState::kNotInIdlePeriod;
}

bool IdleHelper::IsInLongIdlePeriod() const {
  helper_->CheckOnValidThread();
  return idle_period_state_ == IdleHelper::IdlePeriodState::kInLongIdlePeriod ||
         idle_period_state_ ==
             IdleHelper::IdlePeriodState::kInLongIdlePeriodPaused;
}

void IdleHelper::SetIdlePeriodState(IdlePeriodState new_state,
                                    base::TimeTicks new_deadline,
                                    base::TimeTicks optional_now) {
  helper_->CheckOnValidThread();
  if (new_state == idle_period_state_) {
    DCHECK_EQ(new_deadline, idle_period_deadline_);
    return;
  }

  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("renderer.scheduler", &is_tracing);
  if (is_tracing) {
    base::TimeTicks now(optional_now.is_null() ? helper_->NowTicks()
                                               : optional_now);
    TraceEventIdlePeriodStateChange(new_state, running_idle_task_for_tracing_,
                                    new_deadline, now);
  }

  idle_period_state_ = new_state;
  idle_period_deadline_ = new_deadline;
}

void IdleHelper::TraceIdleIdleTaskStart() {
  helper_->CheckOnValidThread();

  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("renderer.scheduler", &is_tracing);
  if (is_tracing) {
    TraceEventIdlePeriodStateChange(idle_period_state_, true,
                                    idle_period_deadline_,
                                    base::TimeTicks::Now());
  }
}

void IdleHelper::TraceIdleIdleTaskEnd() {
  helper_->CheckOnValidThread();

  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("renderer.scheduler", &is_tracing);
  if (is_tracing) {
    TraceEventIdlePeriodStateChange(idle_period_state_, false,
                                    idle_period_deadline_,
                                    base::TimeTicks::Now());
  }
}

void IdleHelper::TraceEventIdlePeriodStateChange(IdlePeriodState new_state,
                                                 bool new_running_idle_task,
                                                 base::TimeTicks new_deadline,
                                                 base::TimeTicks now) {
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "SetIdlePeriodState", "old_state",
               IdleHelper::IdlePeriodStateToString(idle_period_state_),
               "new_state", IdleHelper::IdlePeriodStateToString(new_state));

  if (idle_period_trace_event_started_ && running_idle_task_for_tracing_ &&
      !new_running_idle_task) {
    running_idle_task_for_tracing_ = false;
    if (!idle_period_deadline_.is_null() && now > idle_period_deadline_) {
      if (last_sub_trace_event_name_) {
        TRACE_EVENT_END("renderer.scheduler",
                        perfetto::Track(reinterpret_cast<uint64_t>(this)));
      }
      last_sub_trace_event_name_ = "DeadlineOverrun";
      TRACE_EVENT_BEGIN(
          "renderer.scheduler",
          perfetto::StaticString(last_sub_trace_event_name_),
          perfetto::Track(reinterpret_cast<uint64_t>(this)),
          std::max(idle_period_deadline_, last_idle_task_trace_time_));
    }
  }

  if (new_state != IdlePeriodState::kNotInIdlePeriod) {
    if (!idle_period_trace_event_started_) {
      idle_period_trace_event_started_ = true;
      TRACE_EVENT_BEGIN("renderer.scheduler",
                        perfetto::StaticString(idle_period_tracing_name_),
                        perfetto::Track(reinterpret_cast<uint64_t>(this)),
                        "idle_period_length_ms",
                        (new_deadline - now).InMillisecondsF());
    }

    const char* new_sub_trace_event_name = nullptr;

    if (new_running_idle_task) {
      last_idle_task_trace_time_ = now;
      running_idle_task_for_tracing_ = true;
      new_sub_trace_event_name = "RunningIdleTask";
    } else {
      switch (new_state) {
        case IdlePeriodState::kInShortIdlePeriod:
          new_sub_trace_event_name = "ShortIdlePeriod";
          break;
        case IdlePeriodState::kInLongIdlePeriod:
          new_sub_trace_event_name = "LongIdlePeriod";
          break;
        case IdlePeriodState::kInLongIdlePeriodPaused:
          new_sub_trace_event_name = "LongIdlePeriodPaused";
          break;
        case IdlePeriodState::kNotInIdlePeriod:
          // No sub trace event.
          break;
      }
    }

    if (new_sub_trace_event_name) {
      if (last_sub_trace_event_name_) {
        TRACE_EVENT_END("renderer.scheduler",
                        perfetto::Track(reinterpret_cast<uint64_t>(this)));
      }
      TRACE_EVENT_BEGIN("renderer.scheduler",
                        perfetto::StaticString(new_sub_trace_event_name),
                        perfetto::Track(reinterpret_cast<uint64_t>(this)));
      last_sub_trace_event_name_ = new_sub_trace_event_name;
    }
  } else if (idle_period_trace_event_started_) {
    if (last_sub_trace_event_name_) {
      TRACE_EVENT_END("renderer.scheduler",
                      perfetto::Track(reinterpret_cast<uint64_t>(this)));
      last_sub_trace_event_name_ = nullptr;
    }
    TRACE_EVENT_END("renderer.scheduler",
                    perfetto::Track(reinterpret_cast<uint64_t>(this)));
    idle_period_trace_event_started_ = false;
  }
}

const char* IdleHelper::IdlePeriodStateForTracing() const {
  helper_->CheckOnValidThread();
  return IdlePeriodStateToString(idle_period_state_);
}

// static
const char* IdleHelper::IdlePeriodStateToString(
    IdlePeriodState idle_period_state) {
  switch (idle_period_state) {
    case IdlePeriodState::kNotInIdlePeriod:
      return "not_in_idle_period";
    case IdlePeriodState::kInShortIdlePeriod:
      return "in_short_idle_period";
    case IdlePeriodState::kInLongIdlePeriod:
      return "in_long_idle_period";
    case IdlePeriodState::kInLongIdlePeriodPaused:
      return "in_long_idle_period_paused";
  }
}

}  // namespace scheduler
}  // namespace blink
