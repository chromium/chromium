// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FAKE_SINGLE_THREAD_TASK_RUNNER_H_
#define MEDIA_BASE_FAKE_SINGLE_THREAD_TASK_RUNNER_H_

#include <map>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"

namespace media {

class FakeSingleThreadTaskRunner final : public base::SingleThreadTaskRunner {
 public:
  explicit FakeSingleThreadTaskRunner(base::SimpleTestTickClock* clock);

  FakeSingleThreadTaskRunner(const FakeSingleThreadTaskRunner&) = delete;
  FakeSingleThreadTaskRunner& operator=(const FakeSingleThreadTaskRunner&) =
      delete;

  void RunTasks();

  // Note: Advances |clock_|.
  void Sleep(base::TimeDelta t);

  // base::SingleThreadTaskRunner implementation.
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) final;

  bool RunsTasksInCurrentSequence() const final;

  // This function is currently not used, and will return false.
  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) final;

 protected:
  ~FakeSingleThreadTaskRunner() final;

 private:
  const raw_ptr<base::SimpleTestTickClock> clock_;

  // A compound key is used to ensure FIFO execution of delayed tasks scheduled
  // for the same point-in-time.  The second part of the key is simply a FIFO
  // sequence number.
  using TaskKey = std::pair<base::TimeTicks, unsigned int>;

  // Note: The std::map data structure was chosen because the entire
  // cast_unittests suite performed 20% faster than when using
  // std::priority_queue.  http://crbug.com/530842
  std::map<TaskKey, base::OnceClosure> tasks_;

  bool fail_on_next_task_;
};

}  // namespace media

#endif  // MEDIA_BASE_FAKE_SINGLE_THREAD_TASK_RUNNER_H_
