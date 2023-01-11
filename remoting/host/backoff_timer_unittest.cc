// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/backoff_timer.h"

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class BackoffTimerTest : public testing::Test {
 public:
  BackoffTimerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~BackoffTimerTest() override = default;

  void IncrementCounter() { ++counter_; }

  void AssertNextDelayAndFastForwardBy(base::TimeDelta delay) {
    ASSERT_EQ(task_environment_.NextMainThreadPendingTaskDelay(), delay);
    task_environment_.FastForwardBy(delay);
  }

  int counter() const { return counter_; }

 private:
  base::test::TaskEnvironment task_environment_;

  int counter_ = 0;
};

TEST_F(BackoffTimerTest, Basic) {
  BackoffTimer backoff_timer;
  ASSERT_FALSE(backoff_timer.IsRunning());

  constexpr base::TimeDelta initial_delay = base::Milliseconds(10);
  constexpr base::TimeDelta max_delay = base::Milliseconds(50);

  backoff_timer.Start(FROM_HERE, initial_delay, max_delay,
                      base::BindRepeating(&BackoffTimerTest::IncrementCounter,
                                          base::Unretained(this)));
  ASSERT_TRUE(backoff_timer.IsRunning());
  ASSERT_EQ(0, counter());

  // The backoff timer always immediately fires without delay.
  AssertNextDelayAndFastForwardBy(base::TimeDelta());
  ASSERT_TRUE(backoff_timer.IsRunning());
  ASSERT_EQ(1, counter());

  // The next delay is equal to the initial delay.
  AssertNextDelayAndFastForwardBy(initial_delay);
  ASSERT_TRUE(backoff_timer.IsRunning());
  ASSERT_EQ(2, counter());

  // The next delay is doubled.
  AssertNextDelayAndFastForwardBy(2 * initial_delay);
  ASSERT_TRUE(backoff_timer.IsRunning());
  ASSERT_EQ(3, counter());

  // The next delay is doubled again.
  AssertNextDelayAndFastForwardBy(4 * initial_delay);
  ASSERT_TRUE(backoff_timer.IsRunning());
  ASSERT_EQ(4, counter());

  // The next delay is clamped to the max delay. Otherwise, it would exceed it.
  ASSERT_GT(8 * initial_delay, max_delay);
  AssertNextDelayAndFastForwardBy(max_delay);
  ASSERT_TRUE(backoff_timer.IsRunning());
  ASSERT_EQ(5, counter());

  // The delay remains constant at the max delay.
  AssertNextDelayAndFastForwardBy(max_delay);
  ASSERT_TRUE(backoff_timer.IsRunning());
  ASSERT_EQ(6, counter());

  backoff_timer.Stop();
  ASSERT_FALSE(backoff_timer.IsRunning());
}

}  // namespace remoting
