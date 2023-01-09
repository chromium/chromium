/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/timer.h"

#include <algorithm>
#include "base/task/delay_policy.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"

namespace blink {

TimerBase::TimerBase(
    scoped_refptr<base::SingleThreadTaskRunner> web_task_runner)
    : web_task_runner_(std::move(web_task_runner))
#if DCHECK_IS_ON()
      ,
      thread_(CurrentThread())
#endif
{
}

TimerBase::~TimerBase() {
  Stop();
}

void TimerBase::Start(base::TimeDelta next_fire_interval,
                      base::TimeDelta repeat_interval,
                      const base::Location& caller,
                      bool precise) {
#if DCHECK_IS_ON()
  DCHECK_EQ(thread_, CurrentThread());
#endif

  location_ = caller;
  repeat_interval_ = repeat_interval;
  delay_policy_ = precise ? base::subtle::DelayPolicy::kPrecise
                          : base::subtle::DelayPolicy::kFlexibleNoSooner;
  SetNextFireTime(next_fire_interval.is_zero()
                      ? base::TimeTicks()
                      : TimerCurrentTimeTicks() + next_fire_interval);
}

void TimerBase::Stop() {
#if DCHECK_IS_ON()
  DCHECK_EQ(thread_, CurrentThread());
#endif

  repeat_interval_ = base::TimeDelta();
  next_fire_time_ = base::TimeTicks::Max();
  delayed_task_handle_.CancelTask();
}

base::TimeDelta TimerBase::NextFireInterval() const {
  DCHECK(IsActive());
  if (next_fire_time_.is_null())
    return base::TimeDelta();
  base::TimeTicks current = TimerCurrentTimeTicks();
  if (next_fire_time_ < current)
    return base::TimeDelta();
  return next_fire_time_ - current;
}

void TimerBase::MoveToNewTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
#if DCHECK_IS_ON()
  DCHECK_EQ(thread_, CurrentThread());
  DCHECK(task_runner->RunsTasksInCurrentSequence());
#endif
  // If the underlying task runner stays the same, ignore it.
  if (web_task_runner_ == task_runner) {
    return;
  }

  bool active = IsActive();
  delayed_task_handle_.CancelTask();
  web_task_runner_ = std::move(task_runner);

  if (!active)
    return;

  base::TimeTicks next_fire_time =
      std::exchange(next_fire_time_, base::TimeTicks::Max());
  SetNextFireTime(next_fire_time);
}

void TimerBase::SetTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* tick_clock) {
  DCHECK(!IsActive());
  web_task_runner_ = std::move(task_runner);
  tick_clock_ = tick_clock;
}

void TimerBase::SetNextFireTime(base::TimeTicks next_fire_time) {
#if DCHECK_IS_ON()
  DCHECK_EQ(thread_, CurrentThread());
#endif
  if (next_fire_time_ != next_fire_time) {
    next_fire_time_ = next_fire_time;

    // Cancel any previously posted task.
    delayed_task_handle_.CancelTask();

    delayed_task_handle_ = web_task_runner_->PostCancelableDelayedTaskAt(
        base::subtle::PostDelayedTaskPassKey(), location_, BindTimerClosure(),
        next_fire_time_, delay_policy_);
  }
}

NO_SANITIZE_ADDRESS
void TimerBase::RunInternal() {
  DCHECK(!delayed_task_handle_.IsValid());

  TRACE_EVENT0("blink", "TimerBase::run");
#if DCHECK_IS_ON()
  DCHECK_EQ(thread_, CurrentThread())
      << "Timer posted by " << location_.function_name() << " "
      << location_.file_name() << " was run on a different thread";
#endif

  if (!repeat_interval_.is_zero()) {
    base::TimeTicks now = TimerCurrentTimeTicks();
    // The next tick is `next_fire_time_ + repeat_interval_`, but if late wakeup
    // happens we could miss ticks. To avoid posting immediate "catch-up" tasks,
    // the next task targets the tick following a minimum interval of
    // repeat_interval_ / 20.
    SetNextFireTime((now + repeat_interval_ / 20)
                        .SnappedToNextTick(next_fire_time_, repeat_interval_));
  } else {
    next_fire_time_ = base::TimeTicks::Max();
  }
  Fired();
}

// static
base::TimeTicks TimerBase::TimerCurrentTimeTicks() const {
  return tick_clock_
             ? tick_clock_->NowTicks()
             : ThreadScheduler::Current()->MonotonicallyIncreasingVirtualTime();
}

}  // namespace blink
