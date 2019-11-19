// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/deadline_task_runner.h"

#include <memory>

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "base/time/tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

class DeadlineTaskRunnerTest : public testing::Test {
 public:
  DeadlineTaskRunnerTest()
      : task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}
  ~DeadlineTaskRunnerTest() override = default;

  void SetUp() override {
    deadline_task_runner_.reset(new DeadlineTaskRunner(
        base::BindRepeating(&DeadlineTaskRunnerTest::TestTask,
                            base::Unretained(this)),
        task_environment_.GetMainThreadTaskRunner()));
    run_times_.clear();
  }

  base::TimeTicks Now() {
    return task_environment_.GetMockTickClock()->NowTicks();
  }

  void TestTask() { run_times_.push_back(Now()); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<DeadlineTaskRunner> deadline_task_runner_;
  std::vector<base::TimeTicks> run_times_;
};

TEST_F(DeadlineTaskRunnerTest, RunOnce) {
  base::TimeTicks start_time = Now();
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(10);
  deadline_task_runner_->SetDeadline(FROM_HERE, delay, Now());
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times_, testing::ElementsAre(start_time + delay));
}

TEST_F(DeadlineTaskRunnerTest, RunTwice) {
  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks deadline1 = Now() + delay1;
  deadline_task_runner_->SetDeadline(FROM_HERE, delay1, Now());
  task_environment_.FastForwardUntilNoTasksRemain();

  base::TimeDelta delay2 = base::TimeDelta::FromMilliseconds(100);
  base::TimeTicks deadline2 = Now() + delay2;
  deadline_task_runner_->SetDeadline(FROM_HERE, delay2, Now());
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times_, testing::ElementsAre(deadline1, deadline2));
}

TEST_F(DeadlineTaskRunnerTest, EarlierDeadlinesTakePrecidence) {
  base::TimeTicks start_time = Now();
  base::TimeDelta delay1 = base::TimeDelta::FromMilliseconds(1);
  base::TimeDelta delay10 = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta delay100 = base::TimeDelta::FromMilliseconds(100);
  deadline_task_runner_->SetDeadline(FROM_HERE, delay100, Now());
  deadline_task_runner_->SetDeadline(FROM_HERE, delay10, Now());
  deadline_task_runner_->SetDeadline(FROM_HERE, delay1, Now());
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times_, testing::ElementsAre(start_time + delay1));
}

TEST_F(DeadlineTaskRunnerTest, LaterDeadlinesIgnored) {
  base::TimeTicks start_time = Now();
  base::TimeDelta delay100 = base::TimeDelta::FromMilliseconds(100);
  base::TimeDelta delay10000 = base::TimeDelta::FromMilliseconds(10000);
  deadline_task_runner_->SetDeadline(FROM_HERE, delay100, Now());
  deadline_task_runner_->SetDeadline(FROM_HERE, delay10000, Now());
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_THAT(run_times_, testing::ElementsAre(start_time + delay100));
}

TEST_F(DeadlineTaskRunnerTest, DeleteDeadlineTaskRunnerAfterPosting) {
  deadline_task_runner_->SetDeadline(
      FROM_HERE, base::TimeDelta::FromMilliseconds(10), Now());

  // Deleting the pending task should cancel it.
  deadline_task_runner_.reset(nullptr);
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(run_times_.empty());
}

}  // namespace scheduler
}  // namespace blink
