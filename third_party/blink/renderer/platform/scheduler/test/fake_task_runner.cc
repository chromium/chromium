// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

namespace blink {
namespace scheduler {

class FakeTaskRunner::Data : public WTF::ThreadSafeRefCounted<Data>,
                             public base::TickClock {
 public:
  Data() = default;
  Data(const Data&) = delete;
  Data& operator=(const Data&) = delete;

  void PostDelayedTask(base::OnceClosure task, base::TimeDelta delay) {
    task_queue_.emplace_back(std::move(task), time_ + delay);
  }

  using PendingTask = FakeTaskRunner::PendingTask;
  Deque<PendingTask>::iterator FindRunnableTask() {
    // TODO(tkent): This should return an item which has the minimum |second|.
    // TODO(pkasting): If this is ordered by increasing time, the call below can
    // be changed to `lower_bound()`, which achieves tkent's TODO above and is
    // more efficient to boot.
    return base::ranges::find_if(task_queue_, [&](const PendingTask& item) {
      return item.second <= time_;
    });
  }

  // base::TickClock:
  base::TimeTicks NowTicks() const override { return time_; }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  Deque<PendingTask> task_queue_;
  base::TimeTicks time_;

 private:
  ~Data() override = default;

  friend ThreadSafeRefCounted<Data>;
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

const base::TickClock* FakeTaskRunner::GetMockTickClock() const {
  return data_.get();
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

bool FakeTaskRunner::PostDelayedTaskAt(
    base::subtle::PostDelayedTaskPassKey,
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeTicks delayed_run_time,
    base::subtle::DelayPolicy deadline_policy) {
  return PostDelayedTask(from_here, std::move(task),
                         delayed_run_time.is_null()
                             ? base::TimeDelta()
                             : delayed_run_time - data_->NowTicks());
}

bool FakeTaskRunner::PostNonNestableDelayedTask(const base::Location& location,
                                                base::OnceClosure task,
                                                base::TimeDelta delay) {
  data_->PostDelayedTask(std::move(task), delay);
  return true;
}

}  // namespace scheduler
}  // namespace blink
