// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/time_clamper.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gin {

TEST(TimeClamperTest, CurrentClockTimeMilliseconds) {
  TimeClamper clamper(1);
  EXPECT_EQ(0u, clamper.ClampToMillis(base::Time::UnixEpoch()));

  // 1 us after unix epoch should round to 0.
  base::Time time = base::Time::UnixEpoch() + base::Microseconds(1);
  EXPECT_EQ(0u, clamper.ClampToMillis(time));

  time = base::Time::UnixEpoch() + base::Milliseconds(1) -
         base::Microseconds(TimeClamper::kResolutionMicros - 1);
  EXPECT_EQ(0u, clamper.ClampToMillis(time));

  time = base::Time::UnixEpoch() + base::Milliseconds(1);
  EXPECT_EQ(1u, clamper.ClampToMillis(time));
}

#if !BUILDFLAG(IS_ANDROID)
TEST(TimeClamperTest, CurrentClockTimeMillisecondsThreshold) {
  // This test assumes a time clamp of 5. If the clamp changes the time values
  // will need to be adjusted.
  ASSERT_EQ(5, TimeClamper::kResolutionMicros);
  TimeClamper clamper(1);
  base::Time time = base::Time::UnixEpoch() + base::Milliseconds(1) -
                    base::Microseconds(TimeClamper::kResolutionMicros);
  EXPECT_EQ(0u, clamper.ClampToMillis(time));

  time = base::Time::UnixEpoch() + base::Milliseconds(1) -
         base::Microseconds(TimeClamper::kResolutionMicros) +
         base::Microseconds(1);
  EXPECT_EQ(0u, clamper.ClampToMillis(time));

  time = base::Time::UnixEpoch() + base::Milliseconds(1) -
         base::Microseconds(TimeClamper::kResolutionMicros) +
         base::Microseconds(2);
  EXPECT_EQ(1u, clamper.ClampToMillis(time));
}

TEST(TimeClamperTest, CurrentClockTimeMillisecondsThresholdNegative) {
  // This test assumes a time clamp of 5. If the clamp changes the time values
  // will need to be adjusted.
  // be adjusted.
  ASSERT_EQ(5, TimeClamper::kResolutionMicros);
  TimeClamper clamper(1);
  base::Time time = base::Time::UnixEpoch() + base::Milliseconds(-1) +
                    base::Microseconds(TimeClamper::kResolutionMicros);
  EXPECT_EQ(0u, clamper.ClampToMillis(time));

  time = base::Time::UnixEpoch() + base::Milliseconds(-1) +
         base::Microseconds(TimeClamper::kResolutionMicros) -
         base::Microseconds(1);
  EXPECT_EQ(0u, clamper.ClampToMillis(time));

  time = base::Time::UnixEpoch() + base::Milliseconds(-1) +
         base::Microseconds(TimeClamper::kResolutionMicros) -
         base::Microseconds(2);
  EXPECT_EQ(static_cast<int64_t>(-1), clamper.ClampToMillis(time));
}

TEST(TimeClamperTest, CurrentClockTimeMillisecondsHighResolution) {
  TimeClamper clamper(1);
  EXPECT_EQ(0, clamper.ClampToMillisHighResolution(base::Time::UnixEpoch()));

  base::Time time = base::Time::UnixEpoch() + base::Microseconds(1);
  EXPECT_EQ(0, clamper.ClampToMillisHighResolution(time));

  time = base::Time::UnixEpoch() +
         base::Microseconds(TimeClamper::kResolutionMicros);
  EXPECT_EQ(0.005, clamper.ClampToMillisHighResolution(time));

  time = base::Time::UnixEpoch() + base::Milliseconds(1) -
         base::Microseconds(TimeClamper::kResolutionMicros - 1);
  EXPECT_EQ(0.995, clamper.ClampToMillisHighResolution(time));

  time = base::Time::UnixEpoch() + base::Milliseconds(1) -
         base::Microseconds(TimeClamper::kResolutionMicros - 3);
  EXPECT_EQ(1, clamper.ClampToMillisHighResolution(time));

  time = base::Time::UnixEpoch() + base::Milliseconds(1);
  EXPECT_EQ(1, clamper.ClampToMillisHighResolution(time));

  time =
      base::Time::UnixEpoch() + base::Milliseconds(1) + base::Microseconds(1);
  EXPECT_EQ(1.005, clamper.ClampToMillisHighResolution(time));
}
#endif

}  // namespace gin
