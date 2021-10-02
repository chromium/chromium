// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/common/rtp_time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

// Tests that conversions between base::TimeDelta and RtpTimeDelta are accurate.
// Note that this implicitly tests the conversions to/from RtpTimeTicks as well
// due to shared implementation.
TEST(RtpTimeDeltaTest, ConversionToAndFromTimeDelta) {
  const int kTimebase = 48000;

  // Origin in both timelines is equivalent.
  ASSERT_EQ(RtpTimeDelta(), RtpTimeDelta::FromTicks(0));
  ASSERT_EQ(RtpTimeDelta(),
            RtpTimeDelta::FromTimeDelta(base::TimeDelta(), kTimebase));
  ASSERT_EQ(base::TimeDelta(),
            RtpTimeDelta::FromTicks(0).ToTimeDelta(kTimebase));

  // Conversions that are exact (i.e., do not require rounding).
  ASSERT_EQ(RtpTimeDelta::FromTicks(480),
            RtpTimeDelta::FromTimeDelta(base::Milliseconds(10), kTimebase));
  ASSERT_EQ(RtpTimeDelta::FromTicks(96000),
            RtpTimeDelta::FromTimeDelta(base::Seconds(2), kTimebase));
  ASSERT_EQ(base::Milliseconds(10),
            RtpTimeDelta::FromTicks(480).ToTimeDelta(kTimebase));
  ASSERT_EQ(base::Seconds(2),
            RtpTimeDelta::FromTicks(96000).ToTimeDelta(kTimebase));

  // Conversions that are approximate (i.e., are rounded).
  for (int error_us = -3; error_us <= +3; ++error_us) {
    ASSERT_EQ(RtpTimeDelta::FromTicks(0),
              RtpTimeDelta::FromTimeDelta(base::Microseconds(0 + error_us),
                                          kTimebase));
    ASSERT_EQ(RtpTimeDelta::FromTicks(1),
              RtpTimeDelta::FromTimeDelta(base::Microseconds(21 + error_us),
                                          kTimebase));
    ASSERT_EQ(RtpTimeDelta::FromTicks(2),
              RtpTimeDelta::FromTimeDelta(base::Microseconds(42 + error_us),
                                          kTimebase));
    ASSERT_EQ(RtpTimeDelta::FromTicks(3),
              RtpTimeDelta::FromTimeDelta(base::Microseconds(63 + error_us),
                                          kTimebase));
    ASSERT_EQ(RtpTimeDelta::FromTicks(4),
              RtpTimeDelta::FromTimeDelta(base::Microseconds(83 + error_us),
                                          kTimebase));
    ASSERT_EQ(RtpTimeDelta::FromTicks(11200000000000),
              RtpTimeDelta::FromTimeDelta(
                  base::Microseconds(INT64_C(233333333333333) + error_us),
                  kTimebase));
  }
  ASSERT_EQ(base::Microseconds(21),
            RtpTimeDelta::FromTicks(1).ToTimeDelta(kTimebase));
  ASSERT_EQ(base::Microseconds(42),
            RtpTimeDelta::FromTicks(2).ToTimeDelta(kTimebase));
  ASSERT_EQ(base::Microseconds(63),
            RtpTimeDelta::FromTicks(3).ToTimeDelta(kTimebase));
  ASSERT_EQ(base::Microseconds(83),
            RtpTimeDelta::FromTicks(4).ToTimeDelta(kTimebase));
  ASSERT_EQ(base::Microseconds(INT64_C(233333333333333)),
            RtpTimeDelta::FromTicks(11200000000000).ToTimeDelta(kTimebase));
}

}  // namespace cast
}  // namespace media
