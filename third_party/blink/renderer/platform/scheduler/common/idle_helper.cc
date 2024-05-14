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

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;

// static
constexpr base::TimeDelta IdleHelper::kMaximumIdlePeriod;

IdleHelper::IdleHelper(
    SchedulerHelper* helper,
    Delegate* delegate,
    const char* idle_period_tracing_name,
    base::TimeDelta required_quiescence_duration_before_long_idle_period,
    TaskQueue* idle_queue)
    : helper_(helper),
      delegate_(delegate),
      idle_queue_(idle_queue),
      state_(helper, delegate, idle_period_tracing_name),
      required_quiescence_duration_before_long_idle_period_(
          required_quiescence_duration_before_long_idle_period),
      is_shutdown_(false) {
  weak_idle_helper_ptr_ = weak_factory_.GetWeakPtr();
  enable_next_long_idle_period_closure_.Reset(base::BindRepeating(
      &IdleHelper::EnableLongIdlePeriod, weak_idle_helper_ptr_));
  on_idle_task_posted_closure_.Reset(base::BindRepeating(
      &IdleHelper::OnIdleTaskPostedOnMainThread, weak_idle_helper_ptr_));
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
        std::min(wake_up->time - now, kMaximumIdlePeriod);
  } else {
    long_idle_period_duration = kMaximumIdlePeriod;
  }

  if (long_idle_period_duration >=
      base::Milliseconds(kMinimumIdlePeriodDurationMillis)) {
    *next_long_idle_period_delay_out = long_idle_period_duration;
    if (!idle_queue_->HasTaskToRunImmediatelyOrReadyDelayedTask())
      return IdlePeriodState::kInLongIdlePeriodPaused;
    if (long_idle_period_duration == kMaximumIdlePeriod)
      return IdlePeriodState::kInLongIdlePeriodWithMaxDeadline;
    return IdlePeriodState::kInLongIdlePeriod;
  } else {
    // If we can't start the idle period yet then try again after wake-up.
    *next_long_idle_period_delay_out =
        base::Milliseconds(kRetryEnableLongIdlePeriodDelayMillis);
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
  if (IsInIdlePeriod(new_idle_period_state)) {
    StartIdlePeriod(new_idle_period_state, now,
                    now + next_long_idle_period_delay);
  } else {
    // Otherwise wait for the next long idle period delay before trying again.
    helper_->ControlTaskRunner()->PostDelayedTask(
        FROM_HERE, enable_next_long_idle_period_closure_.GetCallback(),
        next_long_idle_period_delay);
  }
}

void IdleHelper::StartIdlePeriod(IdlePeriodState new_state,
                                 base::TimeTicks now,
                                 base::TimeTicks idle_period_deadline) {
  DCHECK(!is_shutdown_);
  DCHECK_GT(idle_period_deadline, now);
  helper_->CheckOnValidThread();
  DCHECK(IsInIdlePeriod(new_state));

  // Allow any ready delayed idle tasks to run.
  idle_task_runner_->EnqueueReadyDelayedIdleTasks();

  base::TimeDelta idle_period_duration(idle_period_deadline - now);
  if (idle_period_duration <
      base::Milliseconds(kMinimumIdlePeriodDurationMillis)) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                 "NotStartingIdlePeriodBecauseDeadlineIsTooClose",
                 "idle_period_duration_ms",
                 idle_period_duration.InMillisecondsF());
    return;
  }

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "StartIdlePeriod");
  if (!IsInIdlePeriod(state_.idle_period_state()))
    helper_->AddTaskObserver(this);

  // Use a fence to make sure any idle tasks posted after this point do not run
  // until the next idle period and unblock existing tasks.
  idle_queue_->InsertFence(TaskQueue::InsertFencePosition::kNow);

  state_.UpdateState(new_state, idle_period_deadline, now);
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
  if (!IsInIdlePeriod(state_.idle_period_state()))
    return;

  helper_->RemoveTaskObserver(this);

  // This fence will block any idle tasks from running.
  idle_queue_->InsertFence(TaskQueue::InsertFencePosition::kBeginningOfTime);
  state_.UpdateState(IdlePeriodState::kNotInIdlePeriod, base::TimeTicks(),
                     base::TimeTicks());
}

void IdleHelper::WillProcessTask(const base::PendingTask& pending_task,
                                 bool was_blocked_or_low_priority) {
  DCHECK(!is_shutdown_);
}

void IdleHelper::DidProcessTask(const base::PendingTask& pending_task) {
  helper_->CheckOnValidThread();
  DCHECK(!is_shutdown_);
  DCHECK(IsInIdlePeriod(state_.idle_period_state()));
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "DidProcessTask");
  if (state_.idle_period_state() != IdlePeriodState::kInLongIdlePeriodPaused &&
      helper_->NowTicks() >= state_.idle_period_deadline()) {
    // If the idle period deadline has now been reached, either end the idle
    // period or trigger a new long-idle period.
    if (IsInLongIdlePeriod(state_.idle_period_state())) {
      EnableLongIdlePeriod();
    } else {
      DCHECK(IdlePeriodState::kInShortIdlePeriod == state_.idle_period_state());
      EndIdlePeriod();
    }
  }
}

void IdleHelper::UpdateLongIdlePeriodStateAfterIdleTask() {
  helper_->CheckOnValidThread();
  DCHECK(!is_shutdown_);
  DCHECK(IsInLongIdlePeriod(state_.idle_period_state()));
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "UpdateLongIdlePeriodStateAfterIdleTask");

  if (!idle_queue_->HasTaskToRunImmediatelyOrReadyDelayedTask()) {
    // If there are no more idle tasks then pause long idle period ticks until a
    // new idle task is posted.
    state_.UpdateState(IdlePeriodState::kInLongIdlePeriodPaused,
                       state_.idle_period_deadline(), base::TimeTicks());
  } else if (idle_queue_->BlockedByFence()) {
    // If there is still idle work to do then just start the next idle period.
    base::TimeDelta next_long_idle_period_delay;
    // Ensure that we kick the scheduler at the right time to
    // initiate the next idle period.
    next_long_idle_period_delay = std::max(
        base::TimeDelta(), state_.idle_period_deadline() - helper_->NowTicks());
    if (next_long_idle_period_delay.is_zero()) {
      EnableLongIdlePeriod();
    } else {
      helper_->ControlTaskRunner()->PostDelayedTask(
          FROM_HERE, enable_next_long_idle_period_closure_.GetCallback(),
          next_long_idle_period_delay);
    }
  }
}

base::TimeTicks IdleHelper::CurrentIdleTaskDeadline() const {
  helper_->CheckOnValidThread();
  return state_.idle_period_deadline();
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
  if (state_.idle_period_state() == IdlePeriodState::kInLongIdlePeriodPaused) {
    // Restart long idle period ticks.
    helper_->ControlTaskRunner()->PostTask(
        FROM_HERE, enable_next_long_idle_period_closure_.GetCallback());
  }
}

base::TimeTicks IdleHelper::WillProcessIdleTask() {
  helper_->CheckOnValidThread();
  DCHECK(!is_shutdown_);
  state_.TraceIdleIdleTaskStart();
  return CurrentIdleTaskDeadline();
}

void IdleHelper::DidProcessIdleTask() {
  helper_->CheckOnValidThread();
  if (is_shutdown_)
    return;
  state_.TraceIdleIdleTaskEnd();
  if (IsInLongIdlePeriod(state_.idle_period_state())) {
    UpdateLongIdlePeriodStateAfterIdleTask();
  }
  delegate_->OnPendingTasksChanged(idle_queue_->GetNumberOfPendingTasks() > 0);
}

base::TimeTicks IdleHelper::NowTicks() {
  return helper_->NowTicks();
}

// static
bool IdleHelper::IsInIdlePeriod(IdlePeriodState state) {
  return state != IdlePeriodState::kNotInIdlePeriod;
}

// static
bool IdleHelper::IsInLongIdlePeriod(IdlePeriodState state) {
  return state == IdlePeriodState::kInLongIdlePeriod ||
         state == IdlePeriodState::kInLongIdlePeriodWithMaxDeadline ||
         state == IdlePeriodState::kInLongIdlePeriodPaused;
}

bool IdleHelper::CanExceedIdleDeadlineIfRequired() const {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "CanExceedIdleDeadlineIfRequired");
  helper_->CheckOnValidThread();
  return state_.idle_period_state() ==
         IdlePeriodState::kInLongIdlePeriodWithMaxDeadline;
}

IdleHelper::IdlePeriodState IdleHelper::SchedulerIdlePeriodState() const {
  return state_.idle_period_state();
}

IdleHelper::State::State(SchedulerHelper* helper,
                         Delegate* delegate,
                         const char* idle_period_tracing_name)
    : helper_(helper),
      delegate_(delegate),
      idle_period_state_(IdlePeriodState::kNotInIdlePeriod),
      idle_period_trace_event_started_(false),
      running_idle_task_for_tracing_(false),
      idle_period_tracing_name_(idle_period_tracing_name) {}

IdleHelper::State::~State() = default;

IdleHelper::IdlePeriodState IdleHelper::State::idle_period_state() const {
  helper_->CheckOnValidThread();
  return idle_period_state_;
}

base::TimeTicks IdleHelper::State::idle_period_deadline() const {
  helper_->CheckOnValidThread();
  return idle_period_deadline_;
}

void IdleHelper::State::UpdateState(IdlePeriodState new_state,
                                    base::TimeTicks new_deadline,
                                    base::TimeTicks optional_now) {
  IdlePeriodState old_idle_period_state = idle_period_state_;

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

  // Inform the delegate if we are starting or ending an idle period.
  if (IsInIdlePeriod(new_state) && !IsInIdlePeriod(old_idle_period_state)) {
    delegate_->OnIdlePeriodStarted();
  } else if (!IsInIdlePeriod(new_state) &&
             IsInIdlePeriod(old_idle_period_state)) {
    delegate_->OnIdlePeriodEnded();
  }
}

void IdleHelper::State::TraceIdleIdleTaskStart() {
  helper_->CheckOnValidThread();

  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("renderer.scheduler", &is_tracing);
  if (is_tracing) {
    TraceEventIdlePeriodStateChange(idle_period_state_, true,
                                    idle_period_deadline_,
                                    base::TimeTicks::Now());
  }
}

void IdleHelper::State::TraceIdleIdleTaskEnd() {
  helper_->CheckOnValidThread();

  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("renderer.scheduler", &is_tracing);
  if (is_tracing) {
    TraceEventIdlePeriodStateChange(idle_period_state_, false,
                                    idle_period_deadline_,
                                    base::TimeTicks::Now());
  }
}

void IdleHelper::State::TraceEventIdlePeriodStateChange(
    IdlePeriodState new_state,
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
        TRACE_EVENT_NESTABLE_ASYNC_END0("renderer.scheduler",
                                        last_sub_trace_event_name_,
                                        TRACE_ID_LOCAL(this));
      }
      last_sub_trace_event_name_ = "DeadlineOverrun";
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
          "renderer.scheduler", last_sub_trace_event_name_,
          TRACE_ID_LOCAL(this),
          std::max(idle_period_deadline_, last_idle_task_trace_time_));
    }
  }

  if (IsInIdlePeriod(new_state)) {
    if (!idle_period_trace_event_started_) {
      idle_period_trace_event_started_ = true;
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
          "renderer.scheduler", idle_period_tracing_name_, TRACE_ID_LOCAL(this),
          "idle_period_length_ms", (new_deadline - now).InMillisecondsF());
    }

    const char* new_sub_trace_event_name = nullptr;

    if (new_running_idle_task) {
      last_idle_task_trace_time_ = now;
      running_idle_task_for_tracing_ = true;
      new_sub_trace_event_name = "RunningIdleTask";
    } else if (new_state == IdlePeriodState::kInShortIdlePeriod) {
      new_sub_trace_event_name = "ShortIdlePeriod";
    } else if (IsInLongIdlePeriod(new_state) &&
               new_state != IdlePeriodState::kInLongIdlePeriodPaused) {
      new_sub_trace_event_name = "LongIdlePeriod";
    } else if (new_state == IdlePeriodState::kInLongIdlePeriodPaused) {
      new_sub_trace_event_name = "LongIdlePeriodPaused";
    }

    if (new_sub_trace_event_name) {
      if (last_sub_trace_event_name_) {
        TRACE_EVENT_NESTABLE_ASYNC_END0("renderer.scheduler",
                                        last_sub_trace_event_name_,
                                        TRACE_ID_LOCAL(this));
      }
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
          "renderer.scheduler", new_sub_trace_event_name, TRACE_ID_LOCAL(this));
      last_sub_trace_event_name_ = new_sub_trace_event_name;
    }
  } else if (idle_period_trace_event_started_) {
    if (last_sub_trace_event_name_) {
      TRACE_EVENT_NESTABLE_ASYNC_END0("renderer.scheduler",
                                      last_sub_trace_event_name_,
                                      TRACE_ID_LOCAL(this));
      last_sub_trace_event_name_ = nullptr;
    }
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "renderer.scheduler", idle_period_tracing_name_, TRACE_ID_LOCAL(this));
    idle_period_trace_event_started_ = false;
  }
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
    case IdlePeriodState::kInLongIdlePeriodWithMaxDeadline:
      return "in_long_idle_period_with_max_deadline";
    case IdlePeriodState::kInLongIdlePeriodPaused:
      return "in_long_idle_period_paused";
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

}  // namespace scheduler
}  // namespace blink
