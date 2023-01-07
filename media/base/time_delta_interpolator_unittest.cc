// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/time_delta_interpolator.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class TimeDeltaInterpolatorTest : public ::testing::Test {
 public:
  TimeDeltaInterpolatorTest() : interpolator_(&test_tick_clock_) {
    interpolator_.SetPlaybackRate(1.0);
  }

 protected:
  void AdvanceSystemTime(base::TimeDelta delta) {
    test_tick_clock_.Advance(delta);
  }

  base::SimpleTestTickClock test_tick_clock_;
  TimeDeltaInterpolator interpolator_;
};

TEST_F(TimeDeltaInterpolatorTest, Created) {
  const base::TimeDelta kExpected = base::Seconds(0);
  EXPECT_EQ(kExpected, interpolator_.GetInterpolatedTime());
}

TEST_F(TimeDeltaInterpolatorTest, StartInterpolating_NormalSpeed) {
  const base::TimeDelta kZero;
  const base::TimeDelta kTimeToAdvance = base::Seconds(2);

  EXPECT_EQ(kZero, interpolator_.StartInterpolating());
  AdvanceSystemTime(kTimeToAdvance);
  EXPECT_EQ(kTimeToAdvance, interpolator_.GetInterpolatedTime());
}

TEST_F(TimeDeltaInterpolatorTest, StartInterpolating_DoubleSpeed) {
  const base::TimeDelta kZero;
  const base::TimeDelta kTimeToAdvance = base::Seconds(5);

  interpolator_.SetPlaybackRate(2.0);
  EXPECT_EQ(kZero, interpolator_.StartInterpolating());
  AdvanceSystemTime(kTimeToAdvance);
  EXPECT_EQ(2 * kTimeToAdvance, interpolator_.GetInterpolatedTime());
}

TEST_F(TimeDeltaInterpolatorTest, StartInterpolating_HalfSpeed) {
  const base::TimeDelta kZero;
  const base::TimeDelta kTimeToAdvance = base::Seconds(4);

  interpolator_.SetPlaybackRate(0.5);
  EXPECT_EQ(kZero, interpolator_.StartInterpolating());
  AdvanceSystemTime(kTimeToAdvance);
  EXPECT_EQ(kTimeToAdvance / 2, interpolator_.GetInterpolatedTime());
}

TEST_F(TimeDeltaInterpolatorTest, StartInterpolating_ZeroSpeed) {
  // We'll play for 2 seconds at normal speed, 4 seconds at zero speed, and 8
  // seconds at normal speed.
  const base::TimeDelta kZero;
  const base::TimeDelta kPlayDuration1 = base::Seconds(2);
  const base::TimeDelta kPlayDuration2 = base::Seconds(4);
  const base::TimeDelta kPlayDuration3 = base::Seconds(8);
  const base::TimeDelta kExpected = kPlayDuration1 + kPlayDuration3;

  EXPECT_EQ(kZero, interpolator_.StartInterpolating());

  AdvanceSystemTime(kPlayDuration1);
  interpolator_.SetPlaybackRate(0.0);
  AdvanceSystemTime(kPlayDuration2);
  interpolator_.SetPlaybackRate(1.0);
  AdvanceSystemTime(kPlayDuration3);

  EXPECT_EQ(kExpected, interpolator_.GetInterpolatedTime());
}

TEST_F(TimeDeltaInterpolatorTest, StartInterpolating_MultiSpeed) {
  // We'll play for 2 seconds at half speed, 4 seconds at normal speed, and 8
  // seconds at double speed.
  const base::TimeDelta kZero;
  const base::TimeDelta kPlayDuration1 = base::Seconds(2);
  const base::TimeDelta kPlayDuration2 = base::Seconds(4);
  const base::TimeDelta kPlayDuration3 = base::Seconds(8);
  const base::TimeDelta kExpected =
      kPlayDuration1 / 2 + kPlayDuration2 + 2 * kPlayDuration3;

  interpolator_.SetPlaybackRate(0.5);
  EXPECT_EQ(kZero, interpolator_.StartInterpolating());
  AdvanceSystemTime(kPlayDuration1);

  interpolator_.SetPlaybackRate(1.0);
  AdvanceSystemTime(kPlayDuration2);

  interpolator_.SetPlaybackRate(2.0);
  AdvanceSystemTime(kPlayDuration3);
  EXPECT_EQ(kExpected, interpolator_.GetInterpolatedTime());
}

TEST_F(TimeDeltaInterpolatorTest, StopInterpolating) {
  const base::TimeDelta kZero;
  const base::TimeDelta kPlayDuration = base::Seconds(4);
  const base::TimeDelta kPauseDuration = base::Seconds(20);
  const base::TimeDelta kExpectedFirstPause = kPlayDuration;
  const base::TimeDelta kExpectedSecondPause = 2 * kPlayDuration;

  // Play for 4 seconds.
  EXPECT_EQ(kZero, interpolator_.StartInterpolating());
  AdvanceSystemTime(kPlayDuration);

  // Pause for 20 seconds.
  EXPECT_EQ(kExpectedFirstPause, interpolator_.StopInterpolating());
  EXPECT_EQ(kExpectedFirstPause, interpolator_.GetInterpolatedTime());
  AdvanceSystemTime(kPauseDuration);
  EXPECT_EQ(kExpectedFirstPause, interpolator_.GetInterpolatedTime());

  // Play again for 4 more seconds.
  EXPECT_EQ(kExpectedFirstPause, interpolator_.StartInterpolating());
  AdvanceSystemTime(kPlayDuration);
  EXPECT_EQ(kExpectedSecondPause, interpolator_.StopInterpolating());
  EXPECT_EQ(kExpectedSecondPause, interpolator_.GetInterpolatedTime());
}

TEST_F(TimeDeltaInterpolatorTest, SetBounds_Stopped) {
  const base::TimeDelta kFirstTime = base::Seconds(4);
  const base::TimeDelta kSecondTime = base::Seconds(16);
  const base::TimeDelta kArbitraryMaxTime = base::Seconds(100);

  interpolator_.SetBounds(kFirstTime, kArbitraryMaxTime,
                          test_tick_clock_.NowTicks());
  EXPECT_EQ(kFirstTime, interpolator_.GetInterpolatedTime());
  interpolator_.SetBounds(kSecondTime, kArbitraryMaxTime,
                          test_tick_clock_.NowTicks());
  EXPECT_EQ(kSecondTime, interpolator_.GetInterpolatedTime());
}

TEST_F(TimeDeltaInterpolatorTest, SetBounds_Started) {
  // We'll play for 4 seconds, then set the time to 12, then play for 4 more
  // seconds.
  const base::TimeDelta kZero;
  const base::TimeDelta kPlayDuration = base::Seconds(4);
  const base::TimeDelta kUpdatedTime = base::Seconds(12);
  const base::TimeDelta kArbitraryMaxTime = base::Seconds(100);
  const base::TimeDelta kExpected = kUpdatedTime + kPlayDuration;

  EXPECT_EQ(kZero, interpolator_.StartInterpolating());
  AdvanceSystemTime(kPlayDuration);

  interpolator_.SetBounds(kUpdatedTime, kArbitraryMaxTime,
                          test_tick_clock_.NowTicks());
  AdvanceSystemTime(kPlayDuration);
  EXPECT_EQ(kExpected, interpolator_.GetInterpolatedTime());
}

TEST_F(TimeDeltaInterpolatorTest, SetUpperBound) {
  const base::TimeDelta kZero;
  const base::TimeDelta kTimeInterval = base::Seconds(4);
  const base::TimeDelta kMaxTime = base::Seconds(6);

  EXPECT_EQ(kZero, interpolator_.StartInterpolating());
  interpolator_.SetUpperBound(kMaxTime);
  AdvanceSystemTime(kTimeInterval);
  EXPECT_EQ(kTimeInterval, interpolator_.GetInterpolatedTime());

  AdvanceSystemTime(kTimeInterval);
  EXPECT_EQ(kMaxTime, interpolator_.GetInterpolatedTime());

  AdvanceSystemTime(kTimeInterval);
  EXPECT_EQ(kMaxTime, interpolator_.GetInterpolatedTime());
}

TEST_F(TimeDeltaInterpolatorTest, SetUpperBound_MultipleTimes) {
  const base::TimeDelta kZero;
  const base::TimeDelta kTimeInterval = base::Seconds(4);
  const base::TimeDelta kMaxTime0 = base::Seconds(120);
  const base::TimeDelta kMaxTime1 = base::Seconds(6);
  const base::TimeDelta kMaxTime2 = base::Seconds(12);

  EXPECT_EQ(kZero, interpolator_.StartInterpolating());
  interpolator_.SetUpperBound(kMaxTime0);
  AdvanceSystemTime(kTimeInterval);
  EXPECT_EQ(kTimeInterval, interpolator_.GetInterpolatedTime());

  interpolator_.SetUpperBound(kMaxTime1);
  AdvanceSystemTime(kTimeInterval);
  EXPECT_EQ(kMaxTime1, interpolator_.GetInterpolatedTime());

  AdvanceSystemTime(kTimeInterval);
  EXPECT_EQ(kMaxTime1, interpolator_.GetInterpolatedTime());

  interpolator_.SetUpperBound(kMaxTime2);
  EXPECT_EQ(kMaxTime1, interpolator_.GetInterpolatedTime());

  AdvanceSystemTime(kTimeInterval);
  EXPECT_EQ(kMaxTime1 + kTimeInterval, interpolator_.GetInterpolatedTime());

  AdvanceSystemTime(kTimeInterval);
  EXPECT_EQ(kMaxTime2, interpolator_.GetInterpolatedTime());
}

}  // namespace media
