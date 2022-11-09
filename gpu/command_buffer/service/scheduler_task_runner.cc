// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/scheduler_task_runner.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "base/threading/thread_local.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/scheduler.h"

namespace gpu {

namespace {

base::ThreadLocalPointer<const SchedulerTaskRunner>&
GetCurrentTaskRunnerStorage() {
  static base::NoDestructor<base::ThreadLocalPointer<const SchedulerTaskRunner>>
      runner;
  return *runner;
}

void SetCurrentTaskRunner(const SchedulerTaskRunner* runner) {
  GetCurrentTaskRunnerStorage().Set(runner);
}

const SchedulerTaskRunner* GetCurrentTaskRunner() {
  return GetCurrentTaskRunnerStorage().Get();
}

}  // namespace

SchedulerTaskRunner::SchedulerTaskRunner(Scheduler& scheduler,
                                         SequenceId sequence_id)
    : scheduler_(scheduler), sequence_id_(sequence_id) {}

SchedulerTaskRunner::~SchedulerTaskRunner() = default;

void SchedulerTaskRunner::ShutDown() {
  base::AutoLock lock(lock_);
  is_running_ = false;
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
  if (!is_running_)
    return false;

  CHECK(delay.is_zero());
  scheduler_->ScheduleTask(Scheduler::Task(
      sequence_id_,
      base::BindOnce(&SchedulerTaskRunner::RunTask, this, std::move(task)),
      std::vector<SyncToken>()));
  return true;
}

bool SchedulerTaskRunner::RunsTasksInCurrentSequence() const {
  const SchedulerTaskRunner* current = GetCurrentTaskRunner();
  return current != nullptr && current->sequence_id_ == sequence_id_;
}

void SchedulerTaskRunner::RunTask(base::OnceClosure task) {
  {
    // Handle the case where the sequence was shut down after this task was
    // posted but before it had a chance to run. Note that we don't hold the
    // lock while invoking the task below, since a task may reenter this object
    // to e.g. call ShutDown() or post a new task.
    base::AutoLock lock(lock_);
    if (!is_running_)
      return;
  }

  // Scheduler doesn't nest tasks, so we don't support nesting.
  DCHECK(!GetCurrentTaskRunner());
  SetCurrentTaskRunner(this);
  std::move(task).Run();
  SetCurrentTaskRunner(nullptr);
}

}  // namespace gpu
