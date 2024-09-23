// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/timer_based_tick_provider.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

using ::testing::InSequence;
using ::testing::Invoke;

class TimerBasedTickProviderTest : public ::testing::Test {
 public:
  void EnsureTickAboutToElapse() {
    auto now = base::TimeTicks::Now();
    auto next_tick =
        TimerBasedTickProvider::TimeSnappedToNextTick(now, kTickPeriod);
    task_environment_.FastForwardBy(next_tick - now);
  }

  void EnsureTickJustElapsed() {
    EnsureTickAboutToElapse();
    task_environment_.FastForwardBy(base::Microseconds(1));
  }

  base::TimeTicks SnapToNextTick(base::TimeTicks time) {
    return TimerBasedTickProvider::TimeSnappedToNextTick(time, kTickPeriod);
  }

  TimerBasedTickProvider* tick_provider() { return tick_provider_.get(); }

  static constexpr base::TimeDelta kTickPeriod = base::Milliseconds(10);
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  scoped_refptr<TimerBasedTickProvider> tick_provider_ =
      base::MakeRefCounted<TimerBasedTickProvider>(kTickPeriod);
};

TEST_F(TimerBasedTickProviderTest, SnapsTimeToNextTick) {
  constexpr auto kTickPeriod = TimerBasedTickProviderTest::kTickPeriod;
  auto now = base::TimeTicks::Now();
  auto next_tick =
      TimerBasedTickProvider::TimeSnappedToNextTick(now, kTickPeriod);
  EXPECT_EQ(next_tick, TimerBasedTickProvider::TimeSnappedToNextTick(
                           next_tick, kTickPeriod));
  EXPECT_EQ(next_tick + kTickPeriod,
            TimerBasedTickProvider::TimeSnappedToNextTick(
                next_tick + base::Microseconds(1), kTickPeriod));
}

TEST_F(TimerBasedTickProviderTest, InvokesRequestedCallbackOnTick) {
  base::MockOnceCallback<void()> callback;
  EnsureTickAboutToElapse();
  auto start_time = base::TimeTicks::Now();
  auto expected_run_time = SnapToNextTick(start_time);
  EXPECT_EQ(expected_run_time, start_time);
  EXPECT_CALL(callback, Run).WillOnce(Invoke([&] {
    EXPECT_EQ(base::TimeTicks::Now(), expected_run_time);
  }));
  tick_provider()->RequestCallOnNextTick(callback.Get());
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(TimerBasedTickProviderTest, InvokesRequestedCallbackAfterTickElapsed) {
  base::MockOnceCallback<void()> callback;
  EnsureTickJustElapsed();
  auto start_time = base::TimeTicks::Now();
  auto expected_run_time = SnapToNextTick(start_time);
  EXPECT_GT(expected_run_time, start_time);
  EXPECT_CALL(callback, Run).WillOnce(Invoke([&] {
    EXPECT_EQ(base::TimeTicks::Now(), expected_run_time);
  }));
  tick_provider()->RequestCallOnNextTick(callback.Get());
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(TimerBasedTickProviderTest, InvokesTwoCallbacksOnSameTick) {
  base::MockOnceCallback<void()> callback;
  EnsureTickJustElapsed();
  auto expected_run_time = SnapToNextTick(base::TimeTicks::Now());
  EXPECT_CALL(callback, Run).Times(2).WillRepeatedly(Invoke([&] {
    EXPECT_EQ(base::TimeTicks::Now(), expected_run_time);
  }));
  tick_provider()->RequestCallOnNextTick(callback.Get());
  task_environment_.FastForwardBy(kTickPeriod / 2);
  tick_provider()->RequestCallOnNextTick(callback.Get());
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(TimerBasedTickProviderTest,
       InvokesRequestedCallbackOnTickFromCallbackOnTick) {
  base::MockOnceCallback<void()> callback;
  auto expected_run_time = SnapToNextTick(base::TimeTicks::Now());
  bool first_callback_invoke = true;
  EXPECT_CALL(callback, Run).Times(2).WillRepeatedly(Invoke([&] {
    if (first_callback_invoke)
      tick_provider()->RequestCallOnNextTick(callback.Get());
    first_callback_invoke = false;
    EXPECT_EQ(base::TimeTicks::Now(), expected_run_time);
  }));
  tick_provider()->RequestCallOnNextTick(callback.Get());
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(TimerBasedTickProviderTest,
       InvokesRequestedCallbackOnNextTickFromCallbackOnTick) {
  base::MockOnceCallback<void()> callback;
  auto expected_run_time = SnapToNextTick(
      SnapToNextTick(base::TimeTicks::Now()) + base::Microseconds(1));

  InSequence s;
  EXPECT_CALL(callback, Run).WillOnce(Invoke([&] {
    task_environment_.AdvanceClock(base::Microseconds(1));
    tick_provider()->RequestCallOnNextTick(callback.Get());
  }));
  EXPECT_CALL(callback, Run).WillOnce(Invoke(([&] {
    EXPECT_EQ(base::TimeTicks::Now(), expected_run_time);
  })));
  tick_provider()->RequestCallOnNextTick(callback.Get());
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(TimerBasedTickProviderTest, MultipleTickProvidersAreAligned) {
  auto tick_provider2 =
      base::MakeRefCounted<TimerBasedTickProvider>(kTickPeriod);
  base::MockOnceCallback<void()> callback;
  std::optional<base::TimeTicks> callback_time;

  // Request 2 callbacks that should be called on the same tick instant.
  // Nudge time between the requests to guard against too simplistic
  // implementations.
  EnsureTickJustElapsed();
  tick_provider()->RequestCallOnNextTick(callback.Get());
  task_environment_.FastForwardBy(base::Microseconds(1));
  tick_provider2->RequestCallOnNextTick(callback.Get());
  EXPECT_CALL(callback, Run).Times(2).WillRepeatedly(Invoke([&] {
    if (!callback_time.has_value())
      callback_time = base::TimeTicks::Now();
    else
      EXPECT_EQ(*callback_time, base::TimeTicks::Now());
  }));
  task_environment_.FastForwardUntilNoTasksRemain();
}

}  // namespace
}  // namespace blink
