// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/scheduler_task_runner.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "gpu/command_buffer/service/scheduler.h"

namespace gpu {

namespace {

constinit thread_local const SchedulerTaskRunner* current_task_runner = nullptr;

}  // namespace

SchedulerTaskRunner::SchedulerTaskRunner(Scheduler& scheduler,
                                         SequenceId sequence_id)
    : scheduler_(&scheduler), sequence_id_(sequence_id) {}

SchedulerTaskRunner::~SchedulerTaskRunner() = default;

void SchedulerTaskRunner::ShutDown() {
  base::AutoLock lock(lock_);
  scheduler_ = nullptr;
}

bool SchedulerTaskRunner::PostDelayedTask(const base::Location& from_here,
                                          base::OnceClosure task,
                                          base::TimeDelta delay) {
  return PostNonNestableDelayedTask(from_here, std::move(task), delay);
}

bool SchedulerTaskRunner::PostNonNestableDelayedTask(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  base::AutoLock lock(lock_);
  if (!scheduler_) {
    return false;
  }

  CHECK(delay.is_zero());
  scheduler_->ScheduleTask(
      sequence_id_,
      base::BindOnce(&SchedulerTaskRunner::RunTask, this, std::move(task)));
  return true;
}

bool SchedulerTaskRunner::RunsTasksInCurrentSequence() const {
  return current_task_runner &&
         current_task_runner->sequence_id_ == sequence_id_;
}

void SchedulerTaskRunner::RunTask(base::OnceClosure task) {
  {
    // Handle the case where the sequence was shut down after this task was
    // posted but before it had a chance to run. Note that we don't hold the
    // lock while invoking the task below, since a task may reenter this object
    // to e.g. call ShutDown() or post a new task.
    base::AutoLock lock(lock_);
    if (!scheduler_) {
      return;
    }
  }

  // Scheduler doesn't nest tasks, so we don't support nesting.
  const base::AutoReset<const SchedulerTaskRunner*> resetter(
      &current_task_runner, this, nullptr);
  std::move(task).Run();
}

}  // namespace gpu
