// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/h264_rate_control_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {
// H264RateControlUtilTest class is used for running the tests on the methods
// with global scope defined in media::h264_rate_control_util namespace.
class H264RateControlUtilTest : public testing::Test {
 public:
  H264RateControlUtilTest() = default;
};

// Test Cases

// The test calls all the methods from media::h264_rate_control_util namespace
// and checks the return values.
TEST_F(H264RateControlUtilTest, RunBasicH264RateControllerUtilTest) {
  constexpr uint32_t kQPValue = 24u;
  constexpr float kQStepValue = 0.625f * 16;

  EXPECT_EQ(kQPValue, h264_rate_control_util::QStepSize2QP(kQStepValue));

  EXPECT_EQ(kQStepValue, h264_rate_control_util::QP2QStepSize(kQPValue));

  EXPECT_EQ(base::Seconds(1), h264_rate_control_util::ClampedTimestampDiff(
                                  base::Seconds(1), base::Seconds(0)));

  EXPECT_EQ(base::Seconds(0), h264_rate_control_util::ClampedTimestampDiff(
                                  base::Seconds(0), base::Seconds(1)));

  EXPECT_EQ(base::Minutes(5), h264_rate_control_util::ClampedTimestampDiff(
                                  base::Seconds(400), base::Seconds(0)));
}

}  // namespace

}  // namespace media
