// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/feedback_signal_accumulator.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class FeedbackSignalAccumulatorTest : public ::testing::Test {
 public:
  FeedbackSignalAccumulatorTest()
      : half_life_(base::Seconds(1)),
        acc_(half_life_),
        t_(base::TimeTicks() + base::Seconds(120)) {
    acc_.Reset(0.0, t_);
  }

 protected:
  const base::TimeDelta half_life_;
  FeedbackSignalAccumulator<base::TimeTicks> acc_;
  base::TimeTicks t_;
};

TEST_F(FeedbackSignalAccumulatorTest, HasCorrectStartingValueAfterReset) {
  ASSERT_EQ(0.0, acc_.current());
  ASSERT_EQ(t_, acc_.reset_time());
  ASSERT_EQ(t_, acc_.update_time());

  acc_.Reset(1.0, t_);
  ASSERT_EQ(1.0, acc_.current());
  ASSERT_EQ(t_, acc_.reset_time());
  ASSERT_EQ(t_, acc_.update_time());

  t_ += half_life_;
  acc_.Reset(2.0, t_);
  ASSERT_EQ(2.0, acc_.current());
  ASSERT_EQ(t_, acc_.reset_time());
  ASSERT_EQ(t_, acc_.update_time());
}

TEST_F(FeedbackSignalAccumulatorTest, DoesNotUpdateIfBeforeResetTime) {
  acc_.Reset(0.0, t_);
  ASSERT_EQ(0.0, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());

  const base::TimeTicks one_usec_before = t_ - base::Microseconds(1);
  ASSERT_FALSE(acc_.Update(1.0, one_usec_before));
  ASSERT_EQ(0.0, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());

  const base::TimeTicks one_usec_after = t_ + base::Microseconds(1);
  ASSERT_TRUE(acc_.Update(1.0, one_usec_after));
  ASSERT_LT(0.0, acc_.current());
  ASSERT_EQ(one_usec_after, acc_.update_time());
}

TEST_F(FeedbackSignalAccumulatorTest, TakesMaxOfUpdatesAtResetTime) {
  acc_.Reset(0.0, t_);
  ASSERT_EQ(0.0, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());

  ASSERT_TRUE(acc_.Update(1.0, t_));
  ASSERT_EQ(1.0, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());

  ASSERT_TRUE(acc_.Update(2.0, t_));
  ASSERT_EQ(2.0, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());

  ASSERT_TRUE(acc_.Update(1.0, t_));
  ASSERT_EQ(2.0, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
}

TEST_F(FeedbackSignalAccumulatorTest, AppliesMaxOfUpdatesWithSameTimestamp) {
  acc_.Reset(0.0, t_);
  ASSERT_EQ(0.0, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 1 * half_life_;

  // Update with an identical value at the same timestamp.
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(acc_.Update(1.0, t_));
    ASSERT_EQ(0.5, acc_.current());
    ASSERT_EQ(t_, acc_.update_time());
  }

  // Now continue updating with different values at the same timestamp.
  ASSERT_TRUE(acc_.Update(2.0, t_));
  ASSERT_EQ(1.0, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
  ASSERT_TRUE(acc_.Update(3.0, t_));
  ASSERT_EQ(1.5, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
  ASSERT_TRUE(acc_.Update(1.0, t_));
  ASSERT_EQ(1.5, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
}

TEST_F(FeedbackSignalAccumulatorTest, ProvidesExpectedHoldResponse) {
  // Step one half-life interval per update.
  acc_.Reset(0.0, t_);
  ASSERT_EQ(0.0, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 1 * half_life_;
  ASSERT_TRUE(acc_.Update(1.0, t_));
  ASSERT_EQ(0.5, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 1 * half_life_;
  ASSERT_TRUE(acc_.Update(1.0, t_));
  ASSERT_EQ(0.75, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 1 * half_life_;
  ASSERT_TRUE(acc_.Update(1.0, t_));
  ASSERT_EQ(0.875, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 1 * half_life_;
  ASSERT_TRUE(acc_.Update(1.0, t_));
  ASSERT_EQ(0.9375, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());

  // Step two half-life intervals per update.
  acc_.Reset(0.0, t_);
  ASSERT_EQ(0.0, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 2 * half_life_;
  ASSERT_TRUE(acc_.Update(1.0, t_));
  ASSERT_NEAR(0.666666667, acc_.current(), 0.000000001);
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 2 * half_life_;
  ASSERT_TRUE(acc_.Update(1.0, t_));
  ASSERT_NEAR(0.888888889, acc_.current(), 0.000000001);
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 2 * half_life_;
  ASSERT_TRUE(acc_.Update(1.0, t_));
  ASSERT_NEAR(0.962962963, acc_.current(), 0.000000001);
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 2 * half_life_;
  ASSERT_TRUE(acc_.Update(1.0, t_));
  ASSERT_NEAR(0.987654321, acc_.current(), 0.000000001);
  ASSERT_EQ(t_, acc_.update_time());

  // Step three half-life intervals per update.
  acc_.Reset(0.0, t_);
  ASSERT_EQ(0.0, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 3 * half_life_;
  ASSERT_TRUE(acc_.Update(1.0, t_));
  ASSERT_EQ(0.75, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 3 * half_life_;
  ASSERT_TRUE(acc_.Update(1.0, t_));
  ASSERT_EQ(0.9375, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 3 * half_life_;
  ASSERT_TRUE(acc_.Update(1.0, t_));
  ASSERT_EQ(0.984375, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 3 * half_life_;
  ASSERT_TRUE(acc_.Update(1.0, t_));
  ASSERT_EQ(0.99609375, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
}

TEST_F(FeedbackSignalAccumulatorTest, IgnoresUpdatesThatAreOutOfOrder) {
  // First, go forward several steps, in order.
  acc_.Reset(0.0, t_);
  ASSERT_EQ(0.0, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 1 * half_life_;
  ASSERT_TRUE(acc_.Update(2.0, t_));
  ASSERT_EQ(1.0, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 1 * half_life_;
  ASSERT_TRUE(acc_.Update(2.0, t_));
  ASSERT_EQ(1.5, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 1 * half_life_;
  ASSERT_TRUE(acc_.Update(2.0, t_));
  ASSERT_EQ(1.75, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());
  t_ += 1 * half_life_;
  ASSERT_TRUE(acc_.Update(2.0, t_));
  ASSERT_EQ(1.875, acc_.current());
  ASSERT_EQ(t_, acc_.update_time());

  // Go back 1 steps, then 1.5, then 2, then 2.5, etc. and expect the update to
  // fail each time.
  base::TimeTicks earlier = t_ - 1 * half_life_;
  for (int i = 0; i < 5; ++i) {
    ASSERT_FALSE(acc_.Update(999.0, earlier));
    ASSERT_EQ(1.875, acc_.current());
    ASSERT_EQ(t_, acc_.update_time());
    earlier -= half_life_ / 2;
  }
}

}  // namespace media
