// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Common utilities for Quic tests

#ifndef NET_QUIC_TEST_TASK_RUNNER_H_
#define NET_QUIC_TEST_TASK_RUNNER_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner.h"
#include "base/test/test_pending_task.h"
#include "net/third_party/quiche/src/quic/core/quic_time.h"

namespace quic {
class MockClock;
}  // namespace quic
namespace net {

namespace test {

typedef base::TestPendingTask PostedTask;

class TestTaskRunner : public base::SequencedTaskRunner {
 public:
  explicit TestTaskRunner(quic::MockClock* clock);

  // base::TaskRunner implementation.
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override;

  bool RunsTasksInCurrentSequence() const override;

  const std::vector<PostedTask>& GetPostedTasks() const;

  // Returns the delay for next task to run. If there is no pending task,
  // return QuicTime::Delta::Infinite().
  quic::QuicTime::Delta NextPendingTaskDelay();

  // Finds the next task to run, advances the time to the correct time
  // and then runs the task.
  void RunNextTask();

  // Fast forwards virtual time by |delta|, causing all tasks with a remaining
  // delay less than or equal to |delta| to be executed. |delta| must be
  // non-negative.
  void FastForwardBy(quic::QuicTime::Delta delta);

  // While there are posted tasks, finds the next task to run, advances the
  // time to the correct time and then runs the task.
  void RunUntilIdle();

 protected:
  ~TestTaskRunner() override;

 private:
  std::vector<PostedTask>::iterator FindNextTask();

  quic::MockClock* const clock_;
  std::vector<PostedTask> tasks_;

  DISALLOW_COPY_AND_ASSIGN(TestTaskRunner);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TASK_RUNNER_H_
