// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/cancelable_task_scheduler.h"

#include "base/check.h"
#include "base/macros.h"
#include "base/task_runner.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CancelableTaskScheduler::TaskData {
  USING_FAST_MALLOC(TaskData);
  DISALLOW_COPY_AND_ASSIGN(TaskData);

 public:
  TaskData(Task task, CancelableTaskScheduler* scheduler)
      : task_(std::move(task)), scheduler_(scheduler), status_(kWaiting) {}

  ~TaskData() {
    // The task runner is responsible for unregistering the task in case the
    // task hasn't been cancelled.
    if (TryCancel()) {
      scheduler_->UnregisterAndSignal(this);
    }
  }

  void Run() {
    if (TryRun()) {
      std::move(task_).Run();
      scheduler_->UnregisterAndSignal(this);
    }
  }

  bool TryCancel() {
    Status expected = kWaiting;
    return status_.compare_exchange_strong(expected, kCancelled,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire);
  }

 private:
  // Identifies the state a cancelable task is in:
  // |kWaiting|: The task is scheduled and waiting to be executed. {TryRun} will
  // succeed.
  // |kCancelled|: The task has been cancelled. {TryRun} will fail.
  // |kRunning|: The task is currently running and cannot be canceled anymore.
  enum Status : uint8_t { kWaiting, kCancelled, kRunning };

  bool TryRun() {
    Status expected = kWaiting;
    return status_.compare_exchange_strong(expected, kRunning,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire);
  }

  Task task_;
  CancelableTaskScheduler* const scheduler_;
  std::atomic<Status> status_;
};

CancelableTaskScheduler::CancelableTaskScheduler(
    scoped_refptr<base::TaskRunner> task_runner)
    : cond_var_(&lock_), task_runner_(std::move(task_runner)) {}

CancelableTaskScheduler::~CancelableTaskScheduler() {
  base::AutoLock lock(lock_);
  CHECK(tasks_.IsEmpty());
}

void CancelableTaskScheduler::ScheduleTask(Task task) {
  std::unique_ptr<TaskData> task_data = Register(std::move(task));
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&TaskData::Run, std::move(task_data)));
}

size_t CancelableTaskScheduler::CancelAndWait() {
  size_t result = 0;
  base::AutoLock lock(lock_);
  while (!tasks_.IsEmpty()) {
    result += RemoveCancelledTasks();
    if (!tasks_.IsEmpty()) {
      cond_var_.Wait();
    }
  }
  return result;
}

std::unique_ptr<CancelableTaskScheduler::TaskData>
CancelableTaskScheduler::Register(Task task) {
  auto task_data = std::make_unique<TaskData>(std::move(task), this);
  base::AutoLock lock(lock_);
  tasks_.insert(task_data.get());
  return task_data;
}

void CancelableTaskScheduler::UnregisterAndSignal(TaskData* task_data) {
  base::AutoLock lock(lock_);
  CHECK(tasks_.Contains(task_data));
  tasks_.erase(task_data);
  cond_var_.Signal();
}

// This function is needed because WTF::HashSet::erase function invalidates
// all iterators. Returns number of removed tasks.
size_t CancelableTaskScheduler::RemoveCancelledTasks() {
  WTF::Vector<TaskData*> to_be_removed;
  // Assume worst case.
  to_be_removed.ReserveCapacity(tasks_.size());
  for (TaskData* task : tasks_) {
    if (task->TryCancel()) {
      to_be_removed.push_back(task);
    }
  }
  tasks_.RemoveAll(to_be_removed);
  return to_be_removed.size();
}

size_t CancelableTaskScheduler::NumberOfTasksForTesting() const {
  base::AutoLock lock(lock_);
  return tasks_.size();
}

}  // namespace blink
