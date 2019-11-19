// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"

#include <algorithm>
#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {
namespace scheduler {

class FakeTaskRunner::Data : public WTF::ThreadSafeRefCounted<Data> {
 public:
  Data() = default;

  void PostDelayedTask(base::OnceClosure task, base::TimeDelta delay) {
    task_queue_.emplace_back(std::move(task), time_ + delay);
  }

  using PendingTask = FakeTaskRunner::PendingTask;
  Deque<PendingTask>::iterator FindRunnableTask() {
    // TODO(tkent): This should return an item which has the minimum |second|.
    return std::find_if(
        task_queue_.begin(), task_queue_.end(),
        [&](const PendingTask& item) { return item.second <= time_; });
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  Deque<PendingTask> task_queue_;
  base::TimeTicks time_;

 private:
  ~Data() = default;

  friend ThreadSafeRefCounted<Data>;
  DISALLOW_COPY_AND_ASSIGN(Data);
};

FakeTaskRunner::FakeTaskRunner() : data_(base::AdoptRef(new Data)) {}

FakeTaskRunner::FakeTaskRunner(scoped_refptr<Data> data)
    : data_(std::move(data)) {}

FakeTaskRunner::~FakeTaskRunner() = default;

void FakeTaskRunner::SetTime(base::TimeTicks new_time) {
  data_->time_ = new_time;
}

bool FakeTaskRunner::RunsTasksInCurrentSequence() const {
  return true;
}

void FakeTaskRunner::RunUntilIdle() {
  while (!data_->task_queue_.empty()) {
    // Move the task to run into a local variable in case it touches the
    // task queue by posting a new task.
    base::OnceClosure task = std::move(data_->task_queue_.front()).first;
    data_->task_queue_.pop_front();
    std::move(task).Run();
  }
}

void FakeTaskRunner::AdvanceTimeAndRun(base::TimeDelta delta) {
  data_->time_ += delta;
  for (auto it = data_->FindRunnableTask(); it != data_->task_queue_.end();
       it = data_->FindRunnableTask()) {
    base::OnceClosure task = std::move(*it).first;
    data_->task_queue_.erase(it);
    std::move(task).Run();
  }
}

Deque<std::pair<base::OnceClosure, base::TimeTicks>>
FakeTaskRunner::TakePendingTasksForTesting() {
  return std::move(data_->task_queue_);
}

bool FakeTaskRunner::PostDelayedTask(const base::Location& location,
                                     base::OnceClosure task,
                                     base::TimeDelta delay) {
  data_->PostDelayedTask(std::move(task), delay);
  return true;
}

bool FakeTaskRunner::PostNonNestableDelayedTask(const base::Location& location,
                                                base::OnceClosure task,
                                                base::TimeDelta delay) {
  data_->PostDelayedTask(std::move(task), delay);
  return true;
}

}  // namespace scheduler
}  // namespace blink
