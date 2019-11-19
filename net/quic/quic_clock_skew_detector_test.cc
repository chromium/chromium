// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_clock_skew_detector.h"

#include "net/third_party/quiche/src/quic/test_tools/mock_clock.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_random.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

class QuicClockSkewDetectorTest : public ::testing::Test {
 protected:
  QuicClockSkewDetectorTest()
      : start_ticks_time_(base::TimeTicks::Now()),
        start_wall_time_(base::Time::Now()),
        detector_(start_ticks_time_, start_wall_time_) {}

  base::TimeTicks start_ticks_time_;
  base::Time start_wall_time_;
  QuicClockSkewDetector detector_;
};

TEST_F(QuicClockSkewDetectorTest, NoChange) {
  EXPECT_FALSE(
      detector_.ClockSkewDetected(start_ticks_time_, start_wall_time_));
}

TEST_F(QuicClockSkewDetectorTest, NoOffset) {
  base::TimeDelta delta = base::TimeDelta::FromSeconds(57);
  EXPECT_FALSE(detector_.ClockSkewDetected(start_ticks_time_ + delta,
                                           start_wall_time_ + delta));
}

TEST_F(QuicClockSkewDetectorTest, SmallOffset) {
  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(57);
  EXPECT_FALSE(
      detector_.ClockSkewDetected(start_ticks_time_, start_wall_time_ + delta));
}

TEST_F(QuicClockSkewDetectorTest, ManySmallOffset) {
  for (int i = 0; i < 10; ++i) {
    base::TimeDelta delta = base::TimeDelta::FromMilliseconds(500);
    EXPECT_FALSE(detector_.ClockSkewDetected(start_ticks_time_,
                                             start_wall_time_ + i * delta));
  }
}

TEST_F(QuicClockSkewDetectorTest, LargeOffset) {
  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(1001);
  EXPECT_TRUE(
      detector_.ClockSkewDetected(start_ticks_time_, start_wall_time_ + delta));
}

TEST_F(QuicClockSkewDetectorTest, LargeOffsetThenSmallOffset) {
  base::TimeDelta delta = base::TimeDelta::FromMilliseconds(1001);
  EXPECT_TRUE(
      detector_.ClockSkewDetected(start_ticks_time_, start_wall_time_ + delta));
  base::TimeDelta small_delta = base::TimeDelta::FromMilliseconds(571001);
  EXPECT_FALSE(detector_.ClockSkewDetected(
      start_ticks_time_ + small_delta, start_wall_time_ + delta + small_delta));
}

}  // namespace
}  // namespace test
}  // namespace net
