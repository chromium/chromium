// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/wall_clock_time_source.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class WallClockTimeSourceTest : public testing::Test {
 public:
  WallClockTimeSourceTest()
      : tick_clock_(new base::SimpleTestTickClock()),
        time_source_(tick_clock_.get()) {
    AdvanceTimeInSeconds(1);
  }

  WallClockTimeSourceTest(const WallClockTimeSourceTest&) = delete;
  WallClockTimeSourceTest& operator=(const WallClockTimeSourceTest&) = delete;

  ~WallClockTimeSourceTest() override = default;

  void AdvanceTimeInSeconds(int seconds) {
    tick_clock_->Advance(base::Seconds(seconds));
  }

  int CurrentMediaTimeInSeconds() {
    return time_source_.CurrentMediaTime().InSeconds();
  }

  void SetMediaTimeInSeconds(int seconds) {
    return time_source_.SetMediaTime(base::Seconds(seconds));
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
           ConvertMediaTime(base::Seconds(seconds), &is_time_moving);
  }

  bool IsTimeStopped() {
    bool is_time_moving = false;
    // Convert any random value, it shouldn't matter for this call.
    ConvertMediaTime(base::Seconds(1), &is_time_moving);
    return !is_time_moving;
  }

 protected:
  std::unique_ptr<base::SimpleTestTickClock> tick_clock_;
  WallClockTimeSource time_source_;
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
  const base::TimeDelta kOneSecond = base::Seconds(1);
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

TEST_F(WallClockTimeSourceTest, CoarseMediaTimeWhenTicking) {
  time_source_.StartTicking();

  // Initially 0.
  EXPECT_EQ(base::TimeDelta(), time_source_.CurrentMediaTime());

  // Advance by less than kCoarseResolution (e.g., 50 us); should floor to 0.
  tick_clock_->Advance(base::Microseconds(50));
  EXPECT_EQ(base::TimeDelta(), time_source_.CurrentMediaTime());

  // Advance up to 99 us; should still floor to 0.
  tick_clock_->Advance(base::Microseconds(49));
  EXPECT_EQ(base::TimeDelta(), time_source_.CurrentMediaTime());

  // Advance to 100 us; now reaches kCoarseResolution.
  tick_clock_->Advance(base::Microseconds(1));
  EXPECT_EQ(WallClockTimeSource::kCoarseResolution,
            time_source_.CurrentMediaTime());

  // Advance to 199 us; still 100 us.
  tick_clock_->Advance(base::Microseconds(99));
  EXPECT_EQ(WallClockTimeSource::kCoarseResolution,
            time_source_.CurrentMediaTime());

  // Advance to 200 us; becomes 200 us.
  tick_clock_->Advance(base::Microseconds(1));
  EXPECT_EQ(WallClockTimeSource::kCoarseResolution * 2,
            time_source_.CurrentMediaTime());
}

TEST_F(WallClockTimeSourceTest, CoarseMediaTimeWhenStopped) {
  time_source_.StartTicking();
  tick_clock_->Advance(base::Microseconds(150));
  time_source_.StopTicking();

  // Stopped at 150 us; CurrentMediaTime() should be floored to 100 us.
  EXPECT_EQ(WallClockTimeSource::kCoarseResolution,
            time_source_.CurrentMediaTime());
}

TEST_F(WallClockTimeSourceTest, CoarseMediaTimeKillSwitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kCoarseWallClockTimeSource);

  time_source_.StartTicking();
  tick_clock_->Advance(base::Microseconds(50));

  // With feature disabled, CurrentMediaTime() returns exact microsecond time.
  EXPECT_EQ(base::Microseconds(50), time_source_.CurrentMediaTime());
}

TEST_F(WallClockTimeSourceTest, CoarseMediaTimeSeekOffset) {
  // Seek to an unaligned timestamp (150 us).
  time_source_.SetMediaTime(base::Microseconds(150));

  // While stopped, CurrentMediaTime() should not jump backwards below the seek
  // target.
  EXPECT_EQ(base::Microseconds(150), time_source_.CurrentMediaTime());

  time_source_.StartTicking();

  // Elapsed 40 us (media time 190 us); still clamped to seek time.
  tick_clock_->Advance(base::Microseconds(40));
  EXPECT_EQ(base::Microseconds(150), time_source_.CurrentMediaTime());

  // Elapsed 50 us (media time 200 us); reaches the next coarse boundary.
  tick_clock_->Advance(base::Microseconds(10));
  EXPECT_EQ(base::Microseconds(200), time_source_.CurrentMediaTime());

  // Elapsed 149 us (media time 299 us); stays at 200 us.
  tick_clock_->Advance(base::Microseconds(99));
  EXPECT_EQ(base::Microseconds(200), time_source_.CurrentMediaTime());

  // Elapsed 150 us (media time 300 us); reaches 300 us.
  tick_clock_->Advance(base::Microseconds(1));
  EXPECT_EQ(base::Microseconds(300), time_source_.CurrentMediaTime());

  // Stop ticking at 350 us. Paused time should be coarsened to 300 us rather
  // than leaking the exact uncoarsened pause time.
  tick_clock_->Advance(base::Microseconds(50));
  time_source_.StopTicking();
  EXPECT_EQ(base::Microseconds(300), time_source_.CurrentMediaTime());
}

TEST_F(WallClockTimeSourceTest, CoarseMediaTimePlaybackRate) {
  // At playback rate 2.0, media time advances at 2x wall clock. Coarsening
  // resolution remains fixed at 100 us in media time.
  time_source_.SetPlaybackRate(2.0);
  time_source_.StartTicking();

  // Wall-clock advance 49 us -> media time 98 us (< 100 us coarse boundary).
  tick_clock_->Advance(base::Microseconds(49));
  EXPECT_EQ(base::TimeDelta(), time_source_.CurrentMediaTime());

  // Wall-clock advance 1 us -> media time 100 us (reaches boundary).
  tick_clock_->Advance(base::Microseconds(1));
  EXPECT_EQ(base::Microseconds(100), time_source_.CurrentMediaTime());

  // Wall-clock advance 49 us -> media time 198 us (< 200 us coarse boundary).
  tick_clock_->Advance(base::Microseconds(49));
  EXPECT_EQ(base::Microseconds(100), time_source_.CurrentMediaTime());

  // Wall-clock advance 1 us -> media time 200 us.
  tick_clock_->Advance(base::Microseconds(1));
  EXPECT_EQ(base::Microseconds(200), time_source_.CurrentMediaTime());

  time_source_.StopTicking();

  // Test high playback rate 16.0: media time advances 16x faster.
  time_source_.SetMediaTime(base::TimeDelta());
  time_source_.SetPlaybackRate(16.0);
  time_source_.StartTicking();

  // Wall-clock advance 6 us -> media time 96 us (< 100 us coarse boundary).
  tick_clock_->Advance(base::Microseconds(6));
  EXPECT_EQ(base::TimeDelta(), time_source_.CurrentMediaTime());

  // Wall-clock advance 1 us (total 7 us wall clock) -> media time 112 us.
  tick_clock_->Advance(base::Microseconds(1));
  EXPECT_EQ(base::Microseconds(100), time_source_.CurrentMediaTime());

  time_source_.StopTicking();

  // Test rate 0.5: media time advances at half the rate of wall clock.
  time_source_.SetMediaTime(base::TimeDelta());
  time_source_.SetPlaybackRate(0.5);
  time_source_.StartTicking();

  // Wall-clock advance 100 us -> media time 50 us (< 100 us coarse boundary).
  tick_clock_->Advance(base::Microseconds(100));
  EXPECT_EQ(base::TimeDelta(), time_source_.CurrentMediaTime());

  // Wall-clock advance 100 us (total 200 us wall clock) -> media time 100 us.
  tick_clock_->Advance(base::Microseconds(100));
  EXPECT_EQ(base::Microseconds(100), time_source_.CurrentMediaTime());
}

}  // namespace media
