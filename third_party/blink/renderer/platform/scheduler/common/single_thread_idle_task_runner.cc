// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/single_thread_idle_task_runner.h"

#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"

namespace blink {
namespace scheduler {

SingleThreadIdleTaskRunner::SingleThreadIdleTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> idle_priority_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> control_task_runner,
    Delegate* delegate)
    : idle_priority_task_runner_(std::move(idle_priority_task_runner)),
      control_task_runner_(std::move(control_task_runner)),
      delegate_(delegate) {
  weak_scheduler_ptr_ = weak_factory_.GetWeakPtr();
}

SingleThreadIdleTaskRunner::~SingleThreadIdleTaskRunner() = default;

SingleThreadIdleTaskRunner::Delegate::Delegate() = default;

SingleThreadIdleTaskRunner::Delegate::~Delegate() = default;

bool SingleThreadIdleTaskRunner::RunsTasksInCurrentSequence() const {
  return idle_priority_task_runner_->RunsTasksInCurrentSequence();
}

void SingleThreadIdleTaskRunner::PostIdleTask(const base::Location& from_here,
                                              IdleTask idle_task) {
  delegate_->OnIdleTaskPosted();
  idle_priority_task_runner_->PostTask(
      from_here, base::BindOnce(&SingleThreadIdleTaskRunner::RunTask,
                                weak_scheduler_ptr_, std::move(idle_task)));
}
void SingleThreadIdleTaskRunner::PostDelayedIdleTask(
    const base::Location& from_here,
    const base::TimeDelta delay,
    IdleTask idle_task) {
  base::TimeTicks delayed_run_time = delegate_->NowTicks() + delay;
  if (RunsTasksInCurrentSequence()) {
    PostDelayedIdleTaskOnAssociatedThread(from_here, delayed_run_time,
                                          std::move(idle_task));
  } else {
    control_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SingleThreadIdleTaskRunner::PostDelayedIdleTaskOnAssociatedThread,
            weak_scheduler_ptr_, from_here, delayed_run_time,
            std::move(idle_task)));
  }
}

void SingleThreadIdleTaskRunner::PostDelayedIdleTaskOnAssociatedThread(
    const base::Location& from_here,
    const base::TimeTicks delayed_run_time,
    IdleTask idle_task) {
  DCHECK(RunsTasksInCurrentSequence());
  delayed_idle_tasks_.emplace(
      delayed_run_time,
      std::make_pair(
          from_here,
          base::BindOnce(&SingleThreadIdleTaskRunner::RunTask,
                         weak_scheduler_ptr_, std::move(idle_task))));
}

void SingleThreadIdleTaskRunner::PostNonNestableIdleTask(
    const base::Location& from_here,
    IdleTask idle_task) {
  delegate_->OnIdleTaskPosted();
  idle_priority_task_runner_->PostNonNestableTask(
      from_here, base::BindOnce(&SingleThreadIdleTaskRunner::RunTask,
                                weak_scheduler_ptr_, std::move(idle_task)));
}

void SingleThreadIdleTaskRunner::EnqueueReadyDelayedIdleTasks() {
  if (delayed_idle_tasks_.empty())
    return;

  base::TimeTicks now = delegate_->NowTicks();
  while (!delayed_idle_tasks_.empty() &&
         delayed_idle_tasks_.begin()->first <= now) {
    idle_priority_task_runner_->PostTask(
        delayed_idle_tasks_.begin()->second.first,
        std::move(delayed_idle_tasks_.begin()->second.second));
    delayed_idle_tasks_.erase(delayed_idle_tasks_.begin());
  }
}

void SingleThreadIdleTaskRunner::RunTask(IdleTask idle_task) {
  base::TimeTicks deadline = delegate_->WillProcessIdleTask();
  TRACE_EVENT1("renderer.scheduler", "SingleThreadIdleTaskRunner::RunTask",
               "allotted_time_ms",
               (deadline - base::TimeTicks::Now()).InMillisecondsF());
  std::move(idle_task).Run(deadline);
  delegate_->DidProcessIdleTask();
}

}  // namespace scheduler
}  // namespace blink
