// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "media/base/wall_clock_time_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class WallClockTimeSourceTest : public testing::Test {
 public:
  WallClockTimeSourceTest() : tick_clock_(new base::SimpleTestTickClock()) {
    time_source_.SetTickClockForTesting(tick_clock_.get());
    AdvanceTimeInSeconds(1);
  }
  ~WallClockTimeSourceTest() override = default;

  void AdvanceTimeInSeconds(int seconds) {
    tick_clock_->Advance(base::TimeDelta::FromSeconds(seconds));
  }

  int CurrentMediaTimeInSeconds() {
    return time_source_.CurrentMediaTime().InSeconds();
  }

  void SetMediaTimeInSeconds(int seconds) {
    return time_source_.SetMediaTime(base::TimeDelta::FromSeconds(seconds));
  }

  base::TimeTicks ConvertMediaTime(base::TimeDelta timestamp,
                                   bool* is_time_moving) {
    std::vector<base::TimeTicks> wall_clock_times;
    *is_time_moving = time_source_.GetWallClockTimes(
        std::vector<base::TimeDelta>(1, timestamp), &wall_clock_times);
    return wall_clock_times[0];
  }

  bool IsWallClockNowForMediaTimeInSeconds(int seconds) {
    bool is_time_moving = false;
    return tick_clock_->NowTicks() ==
           ConvertMediaTime(base::TimeDelta::FromSeconds(seconds),
                            &is_time_moving);
  }

  bool IsTimeStopped() {
    bool is_time_moving = false;
    // Convert any random value, it shouldn't matter for this call.
    ConvertMediaTime(base::TimeDelta::FromSeconds(1), &is_time_moving);
    return !is_time_moving;
  }

 protected:
  WallClockTimeSource time_source_;
  std::unique_ptr<base::SimpleTestTickClock> tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(WallClockTimeSourceTest);
};

TEST_F(WallClockTimeSourceTest, InitialTimeIsZero) {
  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsTimeStopped());
}

TEST_F(WallClockTimeSourceTest, InitialTimeIsNotTicking) {
  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsTimeStopped());
  AdvanceTimeInSeconds(100);
  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsTimeStopped());
}

TEST_F(WallClockTimeSourceTest, InitialPlaybackRateIsOne) {
  time_source_.StartTicking();

  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNowForMediaTimeInSeconds(0));
  AdvanceTimeInSeconds(100);
  EXPECT_EQ(100, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNowForMediaTimeInSeconds(100));
}

TEST_F(WallClockTimeSourceTest, SetMediaTime) {
  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsTimeStopped());
  SetMediaTimeInSeconds(10);
  EXPECT_EQ(10, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsTimeStopped());
  std::vector<base::TimeTicks> wall_clock_times;
  time_source_.GetWallClockTimes(std::vector<base::TimeDelta>(),
                                 &wall_clock_times);
  EXPECT_EQ(base::TimeTicks(), wall_clock_times[0]);
}

TEST_F(WallClockTimeSourceTest, SetPlaybackRate) {
  time_source_.StartTicking();

  time_source_.SetPlaybackRate(0.5);
  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNowForMediaTimeInSeconds(0));
  AdvanceTimeInSeconds(10);
  EXPECT_EQ(5, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNowForMediaTimeInSeconds(5));

  time_source_.SetPlaybackRate(2);
  EXPECT_EQ(5, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNowForMediaTimeInSeconds(5));
  AdvanceTimeInSeconds(10);
  EXPECT_EQ(25, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNowForMediaTimeInSeconds(25));
}

TEST_F(WallClockTimeSourceTest, StopTicking) {
  time_source_.StartTicking();

  EXPECT_EQ(0, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNowForMediaTimeInSeconds(0));
  AdvanceTimeInSeconds(10);
  EXPECT_EQ(10, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsWallClockNowForMediaTimeInSeconds(10));

  time_source_.StopTicking();

  AdvanceTimeInSeconds(10);
  EXPECT_EQ(10, CurrentMediaTimeInSeconds());
  EXPECT_TRUE(IsTimeStopped());
}

TEST_F(WallClockTimeSourceTest, ConvertsTimestampsWhenStopped) {
  const base::TimeDelta kOneSecond = base::TimeDelta::FromSeconds(1);
  bool is_time_moving = false;
  EXPECT_EQ(base::TimeTicks(),
            ConvertMediaTime(base::TimeDelta(), &is_time_moving));
  EXPECT_FALSE(is_time_moving);
  EXPECT_NE(base::TimeTicks(), ConvertMediaTime(kOneSecond, &is_time_moving));
  EXPECT_FALSE(is_time_moving);
  time_source_.StartTicking();
  time_source_.StopTicking();
  EXPECT_EQ(tick_clock_->NowTicks(),
            ConvertMediaTime(base::TimeDelta(), &is_time_moving));
  EXPECT_FALSE(is_time_moving);
  EXPECT_EQ(tick_clock_->NowTicks() + kOneSecond,
            ConvertMediaTime(kOneSecond, &is_time_moving));
  EXPECT_FALSE(is_time_moving);
}

TEST_F(WallClockTimeSourceTest, EmptyMediaTimestampsReturnMediaWallClockTime) {
  std::vector<base::TimeTicks> wall_clock_times;
  bool is_time_moving = time_source_.GetWallClockTimes(
      std::vector<base::TimeDelta>(), &wall_clock_times);
  EXPECT_FALSE(is_time_moving);
  EXPECT_EQ(base::TimeTicks(), wall_clock_times[0]);

  wall_clock_times.clear();
  time_source_.StartTicking();
  is_time_moving = time_source_.GetWallClockTimes(
      std::vector<base::TimeDelta>(), &wall_clock_times);
  EXPECT_TRUE(is_time_moving);
  EXPECT_EQ(tick_clock_->NowTicks(), wall_clock_times[0]);

  wall_clock_times.clear();
  time_source_.StopTicking();
  is_time_moving = time_source_.GetWallClockTimes(
      std::vector<base::TimeDelta>(), &wall_clock_times);
  EXPECT_FALSE(is_time_moving);
  EXPECT_EQ(tick_clock_->NowTicks(), wall_clock_times[0]);

  // Setting media time should clear reference time.
  SetMediaTimeInSeconds(5);
  wall_clock_times.clear();
  is_time_moving = time_source_.GetWallClockTimes(
      std::vector<base::TimeDelta>(), &wall_clock_times);
  EXPECT_FALSE(is_time_moving);
  EXPECT_EQ(base::TimeTicks(), wall_clock_times[0]);
}

}  // namespace media
