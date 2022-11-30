// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/media_position.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media_session {

class MediaPositionTest : public testing::Test {};

TEST_F(MediaPositionTest, TestPositionUpdated) {
  MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  base::TimeTicks now = base::TimeTicks::Now() + base::Seconds(100);
  base::TimeDelta updated_position = media_position.GetPositionAtTime(now);

  EXPECT_EQ(updated_position.InSeconds(), 400);
}

TEST_F(MediaPositionTest, TestPositionUpdatedTwice) {
  MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(200), /*end_of_media=*/false);

  base::TimeTicks now = base::TimeTicks::Now() + base::Seconds(100);
  base::TimeDelta updated_position = media_position.GetPositionAtTime(now);

  EXPECT_EQ(updated_position.InSeconds(), 300);

  now += base::Seconds(100);
  updated_position = media_position.GetPositionAtTime(now);

  EXPECT_EQ(updated_position.InSeconds(), 400);
}

TEST_F(MediaPositionTest, TestPositionUpdatedPastDuration) {
  MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  base::TimeTicks now = base::TimeTicks::Now() + base::Seconds(400);
  base::TimeDelta updated_position = media_position.GetPositionAtTime(now);

  // Verify that the position has been updated to the end of the total duration.
  EXPECT_EQ(updated_position.InSeconds(), 600);
}

TEST_F(MediaPositionTest, TestPositionAtStart) {
  MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(0), /*end_of_media=*/false);

  base::TimeDelta updated_position = media_position.GetPosition();

  EXPECT_EQ(updated_position.InSeconds(), 0);
}

TEST_F(MediaPositionTest, TestNegativePosition) {
  MediaPosition media_position(
      /*playback_rate=*/-1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  base::TimeTicks now = base::TimeTicks::Now() + base::Seconds(400);
  base::TimeDelta updated_position = media_position.GetPositionAtTime(now);

  // Verify that the position does not go below 0.
  EXPECT_EQ(updated_position.InSeconds(), 0);
}

TEST_F(MediaPositionTest, TestPositionUpdatedNoChange) {
  MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  // Get the updated position without moving forward in time.
  base::TimeDelta updated_position = media_position.GetPosition();

  // Verify that the updated position has not changed.
  EXPECT_EQ(updated_position.InSeconds(), 300);
}

TEST_F(MediaPositionTest, TestPositionUpdatedFasterPlayback) {
  MediaPosition media_position(
      /*playback_rate=*/2, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  base::TimeTicks now = base::TimeTicks::Now() + base::Seconds(100);
  base::TimeDelta updated_position = media_position.GetPositionAtTime(now);

  EXPECT_EQ(updated_position.InSeconds(), 500);
}

TEST_F(MediaPositionTest, TestPositionUpdatedSlowerPlayback) {
  MediaPosition media_position(
      /*playback_rate=*/.5, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  base::TimeTicks now = base::TimeTicks::Now() + base::Seconds(200);
  base::TimeDelta updated_position = media_position.GetPositionAtTime(now);

  EXPECT_EQ(updated_position.InSeconds(), 400);
}

TEST_F(MediaPositionTest, TestNotEquals_AllDifferent) {
  EXPECT_NE(MediaPosition(
                /*playback_rate=*/.5, /*duration=*/base::Seconds(600),
                /*position=*/base::Seconds(300),
                /*end_of_media=*/false),
            MediaPosition(
                /*playback_rate=*/1, /*duration=*/base::Seconds(800),
                /*position=*/base::Seconds(100),
                /*end_of_media=*/true));
}

TEST_F(MediaPositionTest, TestNotEquals_DifferentDuration) {
  MediaPosition position_1(
      /*playback_rate=*/.5, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300),
      /*end_of_media=*/false);

  MediaPosition position_2(
      /*playback_rate=*/.5, /*duration=*/base::Seconds(1000),
      /*position=*/base::Seconds(300),
      /*end_of_media=*/false);

  position_1.last_updated_time_ = position_2.last_updated_time_;

  EXPECT_NE(position_1, position_2);
}

TEST_F(MediaPositionTest, TestNotEquals_DifferentPlaybackRate) {
  MediaPosition position_1(
      /*playback_rate=*/.5, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300),
      /*end_of_media=*/false);

  MediaPosition position_2(
      /*playback_rate=*/1.0, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300),
      /*end_of_media=*/false);

  position_1.last_updated_time_ = position_2.last_updated_time_;

  EXPECT_NE(position_1, position_2);
}

TEST_F(MediaPositionTest, TestNotEquals_DifferentEndOfMedia) {
  MediaPosition position_1(
      /*playback_rate=*/.5, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300),
      /*end_of_media=*/false);

  MediaPosition position_2(
      /*playback_rate=*/.5, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300),
      /*end_of_media=*/true);

  position_1.last_updated_time_ = position_2.last_updated_time_;

  EXPECT_NE(position_1, position_2);
}

TEST_F(MediaPositionTest, TestEquals_AllSame) {
  MediaPosition position_1(
      /*playback_rate=*/.5, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300),
      /*end_of_media=*/false);

  MediaPosition position_2(
      /*playback_rate=*/.5, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300),
      /*end_of_media=*/false);

  position_1.last_updated_time_ = position_2.last_updated_time_;

  EXPECT_EQ(position_1, position_2);
}

TEST_F(MediaPositionTest, TestEquals_SameButDifferentTime) {
  MediaPosition position_1(
      /*playback_rate=*/1.0, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(0),
      /*end_of_media=*/false);

  MediaPosition position_2(
      /*playback_rate=*/1.0, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(10),
      /*end_of_media=*/false);

  position_2.last_updated_time_ = position_1.last_updated_time_;
  position_1.last_updated_time_ -= base::Seconds(10);

  EXPECT_EQ(position_1, position_2);
}

TEST_F(MediaPositionTest, TestPosition_TimeHasGoneBackwards) {
  // This test is only valid when base::TimeTicks is not consistent across
  // processes.
  if (base::TimeTicks::IsConsistentAcrossProcesses())
    return;

  MediaPosition media_position(
      /*playback_rate=*/1, /*duration=*/base::Seconds(600),
      /*position=*/base::Seconds(300), /*end_of_media=*/false);

  // Query with a time in the past.
  base::TimeTicks now = base::TimeTicks::Now() - base::Seconds(1);
  base::TimeDelta updated_position = media_position.GetPositionAtTime(now);

  // It should act as if no time has passed.
  EXPECT_EQ(updated_position.InSeconds(), 300);
}

}  // namespace media_session
