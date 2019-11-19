// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/ukm_task_sampler.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock-matchers.h"

namespace blink {
namespace scheduler {

using ::testing::DoubleEq;

TEST(UkmTaskSamplerTest, SamplesAlwaysForProbabilityOne) {
  UkmTaskSampler always_thread_time_sampler(
      /*thread_time_sampling_rate = */ 1.0,
      /*ukm_task_sampling_rate = */ 1.0);
  UkmTaskSampler never_thread_time_sampler(
      /*thread_time_sampling_rate = */ 0.0,
      /*ukm_task_sampling_rate = */ 1.0);

  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(always_thread_time_sampler.ShouldRecordTaskUkm(true));
    EXPECT_TRUE(always_thread_time_sampler.ShouldRecordTaskUkm(false));
    EXPECT_TRUE(never_thread_time_sampler.ShouldRecordTaskUkm(true));
    EXPECT_TRUE(never_thread_time_sampler.ShouldRecordTaskUkm(false));
  }
}

TEST(UkmTaskSamplerTest, NeverSamplesForProbabilityZero) {
  UkmTaskSampler always_thread_time_sampler(
      /*thread_time_sampling_rate = */ 1.0,
      /*ukm_task_sampling_rate = */ 0.0);
  UkmTaskSampler never_thread_time_sampler(
      /*thread_time_sampling_rate = */ 0.0,
      /*ukm_task_sampling_rate = */ 0.0);

  for (int i = 0; i < 10; ++i) {
    EXPECT_FALSE(always_thread_time_sampler.ShouldRecordTaskUkm(true));
    EXPECT_FALSE(always_thread_time_sampler.ShouldRecordTaskUkm(false));
    EXPECT_FALSE(never_thread_time_sampler.ShouldRecordTaskUkm(true));
    EXPECT_FALSE(never_thread_time_sampler.ShouldRecordTaskUkm(false));
  }
}

// Make sure that ukm_prob = ukm_prob_given_time * time_prob +
// ukm_prob_given_no_time * no_time_prob
TEST(UkmTaskSamplerTest, GetConditionalSamplingProbability) {
  for (double time_prob = 0; time_prob < 1.0; time_prob += 0.1) {
    UkmTaskSampler sampler(time_prob);
    for (double expected_ukm_rate = 0; expected_ukm_rate < 1.0;
         expected_ukm_rate += 0.1) {
      sampler.SetUkmTaskSamplingRate(expected_ukm_rate);
      double ukm_rate =
          sampler.GetConditionalSamplingProbability(true) * time_prob +
          sampler.GetConditionalSamplingProbability(false) * (1 - time_prob);
      EXPECT_THAT(ukm_rate, DoubleEq(expected_ukm_rate))
          << "For time_prob: " << time_prob;
    }
  }
}

TEST(UkmTaskSamplerTest, GetConditionalSamplingProbabilityWithEdgeCases) {
  UkmTaskSampler sampler_0_0(/*thread_time_sampling_rate=*/0,
                             /*ukm_task_sampling_rate=*/0);
  EXPECT_EQ(sampler_0_0.GetConditionalSamplingProbability(false), 0.0);
  // This doesn't really make sense given that thread_time_sampling_rate=0, but
  // make sure we support it
  EXPECT_EQ(sampler_0_0.GetConditionalSamplingProbability(true), 0.0);

  UkmTaskSampler sampler_0_1(/*thread_time_sampling_rate=*/0,
                             /*ukm_task_sampling_rate=*/1);
  EXPECT_EQ(sampler_0_1.GetConditionalSamplingProbability(false), 1.0);
  // This doesn't really make sense given that thread_time_sampling_rate=0, but
  // make sure we support it
  EXPECT_EQ(sampler_0_1.GetConditionalSamplingProbability(true), 1.0);

  UkmTaskSampler sampler_1_0(/*thread_time_sampling_rate=*/1,
                             /*ukm_task_sampling_rate=*/0);
  EXPECT_EQ(sampler_1_0.GetConditionalSamplingProbability(true), 0.0);
  // This doesn't really make sense given that thread_time_sampling_rate=1, but
  // make sure we support it
  EXPECT_EQ(sampler_1_0.GetConditionalSamplingProbability(false), 0.0);

  UkmTaskSampler sampler_1_1(/*thread_time_sampling_rate=*/1,
                             /*ukm_task_sampling_rate=*/1);
  EXPECT_EQ(sampler_1_1.GetConditionalSamplingProbability(true), 1.0);
  // This doesn't really make sense given that thread_time_sampling_rate=1, but
  // make sure we support it
  EXPECT_EQ(sampler_1_1.GetConditionalSamplingProbability(false), 1.0);
}

}  // namespace scheduler
}  // namespace blink
