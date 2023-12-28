// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/exponential_moving_average.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

// Test ExponentialMovingAverageTest adds the predefined values and checks
// whether the correct mean and standard deviation values are produced.
class ExponentialMovingAverageTest : public testing::Test {
 public:
  ExponentialMovingAverageTest() = default;

  void SetUp() override {
    moving_average_ =
        std::make_unique<ExponentialMovingAverage>(base::Milliseconds(100));
    EXPECT_EQ(base::TimeDelta(), moving_average_->curr_window_size());
    EXPECT_EQ(base::Milliseconds(100), moving_average_->max_window_size());
  }

 protected:
  std::unique_ptr<ExponentialMovingAverage> moving_average_;
};

// Test Cases

// Adding predefined sequence to the moving average filter and checking
// whether the stats are inside expected ranges. The filter is checked
// with two sequences using different window sizes.
TEST_F(ExponentialMovingAverageTest, RunBasicMovingAverageTest) {
  constexpr float kExpectedMeanMin1 = 94.73f;
  constexpr float kExpectedMeanMax1 = 94.74f;
  constexpr float kExpectedStdDevMin1 = 1.72f;
  constexpr float kExpectedStdDevMax1 = 1.73f;
  constexpr float kExpectedMeanMin2 = 103.16f;
  constexpr float kExpectedMeanMax2 = 103.17f;
  constexpr float kExpectedStdDevMin2 = 3.19f;
  constexpr float kExpectedStdDevMax2 = 3.20f;

  base::TimeDelta timestamp = base::Microseconds(0);
  moving_average_->AddValue(100, timestamp);
  timestamp += base::Milliseconds(10);
  moving_average_->AddValue(120, timestamp);
  timestamp += base::Milliseconds(8);
  moving_average_->AddValue(90, timestamp);
  timestamp += base::Milliseconds(12);
  moving_average_->AddValue(115, timestamp);
  timestamp += base::Milliseconds(11);
  moving_average_->AddValue(95, timestamp);
  timestamp += base::Milliseconds(9);
  moving_average_->AddValue(100, timestamp);
  timestamp += base::Milliseconds(10);
  moving_average_->AddValue(120, timestamp);
  timestamp += base::Milliseconds(11);
  moving_average_->AddValue(115, timestamp);
  timestamp += base::Milliseconds(7);
  moving_average_->AddValue(90, timestamp);
  timestamp += base::Milliseconds(11);
  moving_average_->AddValue(85, timestamp);
  timestamp += base::Milliseconds(8);
  moving_average_->AddValue(95, timestamp);

  EXPECT_LT(kExpectedMeanMin1, moving_average_->mean());
  EXPECT_GT(kExpectedMeanMax1, moving_average_->mean());
  EXPECT_LT(kExpectedStdDevMin1, moving_average_->GetStdDeviation());
  EXPECT_GT(kExpectedStdDevMax1, moving_average_->GetStdDeviation());

  moving_average_->update_max_window_size(base::Milliseconds(200));
  EXPECT_EQ(base::Milliseconds(100), moving_average_->curr_window_size());
  EXPECT_EQ(base::Milliseconds(200), moving_average_->max_window_size());

  moving_average_->AddValue(105, timestamp);
  timestamp += base::Milliseconds(11);
  moving_average_->AddValue(90, timestamp);
  timestamp += base::Milliseconds(11);
  moving_average_->AddValue(100, timestamp);
  timestamp += base::Milliseconds(8);
  moving_average_->AddValue(100, timestamp);
  timestamp += base::Milliseconds(10);
  moving_average_->AddValue(105, timestamp);

  EXPECT_LT(kExpectedMeanMin2, moving_average_->mean());
  EXPECT_GT(kExpectedMeanMax2, moving_average_->mean());
  EXPECT_LT(kExpectedStdDevMin2, moving_average_->GetStdDeviation());
  EXPECT_GT(kExpectedStdDevMax2, moving_average_->GetStdDeviation());
}

}  // namespace

}  // namespace media
