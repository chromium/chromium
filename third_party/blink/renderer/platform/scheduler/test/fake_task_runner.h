// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_FAKE_TASK_RUNNER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_FAKE_TASK_RUNNER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {
namespace scheduler {

// A dummy task runner for tests.
class FakeTaskRunner : public base::SingleThreadTaskRunner {
 public:
  FakeTaskRunner();

  void SetTime(base::TimeTicks new_time);
  void SetTime(double new_time) {
    SetTime(base::TimeTicks() + base::TimeDelta::FromSecondsD(new_time));
  }

  // base::SingleThreadTaskRunner implementation:
  bool RunsTasksInCurrentSequence() const override;

  void RunUntilIdle();
  void AdvanceTimeAndRun(base::TimeDelta delta);
  void AdvanceTimeAndRun(double delta_seconds) {
    AdvanceTimeAndRun(base::TimeDelta::FromSecondsD(delta_seconds));
  }

  using PendingTask = std::pair<base::OnceClosure, base::TimeTicks>;
  Deque<PendingTask> TakePendingTasksForTesting();

 protected:
  bool PostDelayedTask(const base::Location& location,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const base::Location&,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override;

 private:
  ~FakeTaskRunner() override;

  class Data;
  class BaseTaskRunner;
  scoped_refptr<Data> data_;

  explicit FakeTaskRunner(scoped_refptr<Data> data);

  DISALLOW_COPY_AND_ASSIGN(FakeTaskRunner);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_TEST_FAKE_TASK_RUNNER_H_
