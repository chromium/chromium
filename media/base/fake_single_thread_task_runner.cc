// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/fake_single_thread_task_runner.h"

#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/time/tick_clock.h"

namespace media {

FakeSingleThreadTaskRunner::FakeSingleThreadTaskRunner(
    base::SimpleTestTickClock* clock)
    : clock_(clock), fail_on_next_task_(false) {}

FakeSingleThreadTaskRunner::~FakeSingleThreadTaskRunner() = default;

bool FakeSingleThreadTaskRunner::PostDelayedTask(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  if (fail_on_next_task_) {
    LOG(FATAL) << "Infinite task posting loop detected.  Possibly caused by "
               << from_here.ToString() << " posting a task with delay "
               << delay.InMicroseconds() << " usec.";
  }

  CHECK_LE(base::TimeDelta(), delay);
  const base::TimeTicks run_time = clock_->NowTicks() + delay;

  // If there are one or more tasks with the exact same run time, schedule this
  // task to occur after them.  This mimics the FIFO ordering behavior when
  // scheduling delayed tasks to be run via base::MessageLoop in a
  // multi-threaded application.
  if (!tasks_.empty()) {
    const auto after_it =
        tasks_.lower_bound(TaskKey(run_time + base::Microseconds(1), 0));
    if (after_it != tasks_.begin()) {
      auto it = after_it;
      --it;
      if (it->first.first == run_time) {
        tasks_.insert(after_it /* hint */,
                      std::make_pair(TaskKey(run_time, it->first.second + 1),
                                     std::move(task)));
        return true;
      }
    }
  }

  // No tasks have the exact same run time, so just do a simple insert.
  tasks_.insert(std::make_pair(TaskKey(run_time, 0), std::move(task)));
  return true;
}

bool FakeSingleThreadTaskRunner::RunsTasksInCurrentSequence() const {
  return true;
}

void FakeSingleThreadTaskRunner::RunTasks() {
  while (true) {
    // Run all tasks equal or older than current time.
    const auto it = tasks_.begin();
    if (it == tasks_.end())
      return;  // No more tasks.

    if (clock_->NowTicks() < it->first.first)
      return;

    base::OnceClosure task = std::move(it->second);
    tasks_.erase(it);
    std::move(task).Run();
  }
}

void FakeSingleThreadTaskRunner::Sleep(base::TimeDelta t) {
  CHECK_LE(base::TimeDelta(), t);
  const base::TimeTicks run_until = clock_->NowTicks() + t;

  while (true) {
    // Run up to 100000 tasks that were scheduled to run during the sleep
    // period. 100000 should be enough for everybody (see comments below).
    for (int i = 0; i < 100000; i++) {
      const auto it = tasks_.begin();
      if (it == tasks_.end() || run_until < it->first.first) {
        clock_->Advance(run_until - clock_->NowTicks());
        return;
      }

      clock_->Advance(it->first.first - clock_->NowTicks());
      base::OnceClosure task = std::move(it->second);
      tasks_.erase(it);
      std::move(task).Run();
    }

    // If this point is reached, there's likely some sort of case where a new
    // non-delayed task is being posted every time a task is popped and invoked
    // from the queue. If that happens, set fail_on_next_task_ to true and throw
    // an error when the next task is posted, where we might be able to identify
    // the caller causing the problem via logging.
    fail_on_next_task_ = true;
  }
}

bool FakeSingleThreadTaskRunner::PostNonNestableDelayedTask(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace media
