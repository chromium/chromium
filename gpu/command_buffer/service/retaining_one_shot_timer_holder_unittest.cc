// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/retaining_one_shot_timer_holder.h"

#include "base/check.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class RetainingOneShotTimerHolderTest : public testing::Test {
 protected:
  RetainingOneShotTimerHolderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  base::RepeatingClosure CreateTimerCallback() {
    return base::BindLambdaForTesting([this]() {
      CHECK(task_environment_.GetMainThreadTaskRunner()
                ->BelongsToCurrentThread());
      timer_fired_count_++;
    });
  }

  void PostToThreadPoolAndWait(base::OnceClosure task) {
    base::RunLoop run_loop;
    base::ThreadPool::PostTaskAndReply(FROM_HERE, std::move(task),
                                       run_loop.QuitClosure());
    run_loop.Run();
  }

  int timer_fired_count_ = 0;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(RetainingOneShotTimerHolderTest, SameThreadUsage) {
  scoped_refptr<RetainingOneShotTimerHolder> timer(
      new RetainingOneShotTimerHolder(
          /*max_delay=*/base::Milliseconds(100),
          /*min_delay=*/base::Milliseconds(50),
          task_environment_.GetMainThreadTaskRunner(), CreateTimerCallback()));

  timer->ResetTimerIfNecessary();

  task_environment_.FastForwardBy(base::Milliseconds(101));

  EXPECT_EQ(timer_fired_count_, 1);

  timer->ResetTimerIfNecessary();

  task_environment_.FastForwardBy(base::Milliseconds(20));

  // The timer should not reset given that the remaining time to fire is greater
  // than the min delay to reset.
  timer->ResetTimerIfNecessary();

  task_environment_.FastForwardBy(base::Milliseconds(81));

  EXPECT_EQ(timer_fired_count_, 2);

  timer->ResetTimerIfNecessary();

  task_environment_.FastForwardBy(base::Milliseconds(51));

  // The timer should reset because the remaining time to fire is less than the
  // min delay to reset.
  timer->ResetTimerIfNecessary();

  task_environment_.FastForwardBy(base::Milliseconds(50));

  // Verify that the timer hasn't been fired again.
  EXPECT_EQ(timer_fired_count_, 2);

  task_environment_.FastForwardBy(base::Milliseconds(51));

  EXPECT_EQ(timer_fired_count_, 3);

  timer->DestroyTimer();
}

TEST_F(RetainingOneShotTimerHolderTest, MultiThreadUsage) {
  scoped_refptr<RetainingOneShotTimerHolder> timer(
      new RetainingOneShotTimerHolder(
          /*max_delay=*/base::Milliseconds(100),
          /*min_delay=*/base::Milliseconds(50),
          task_environment_.GetMainThreadTaskRunner(), CreateTimerCallback()));

  PostToThreadPoolAndWait(base::BindLambdaForTesting(
      [timer]() { timer->ResetTimerIfNecessary(); }));

  task_environment_.FastForwardBy(base::Milliseconds(101));

  EXPECT_EQ(timer_fired_count_, 1);

  PostToThreadPoolAndWait(base::BindLambdaForTesting(
      [timer]() { timer->ResetTimerIfNecessary(); }));

  task_environment_.FastForwardBy(base::Milliseconds(50));

  PostToThreadPoolAndWait(
      base::BindLambdaForTesting([timer]() { timer->DestroyTimer(); }));

  task_environment_.FastForwardBy(base::Milliseconds(51));

  // The timer has been destroyed and should not be fired again.
  EXPECT_EQ(timer_fired_count_, 1);
}

}  // namespace gpu
