// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_UKM_TASK_SAMPLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_UKM_TASK_SAMPLER_H_

#include <random>

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {
namespace scheduler {

// Helper to determine whether a task should be recorded for UKM. This class
// tries to maximize the probability of recording an UKM sample for tasks that
// are also recording their thread time.
class PLATFORM_EXPORT UkmTaskSampler {
 public:
  static constexpr double kDefaultUkmTaskSamplingRate = 0.0001;

  // Rates must be in the interval [0, 1] and will be clamped otherwise.
  explicit UkmTaskSampler(
      double thread_time_sampling_rate,
      double ukm_task_sampling_rate = kDefaultUkmTaskSamplingRate);

  // Returns true with probability of ukm_task_sampling_rate maximizing the
  // probablility of recording UKMs for tasks that also record thread_time.
  bool ShouldRecordTaskUkm(bool has_thread_time);

  // |rate| must be in the interval [0, 1] and will be clamped otherwise.
  void SetUkmTaskSamplingRate(double rate);

 private:
  // So that we can test GetConditionalSamplingProbability
  FRIEND_TEST_ALL_PREFIXES(UkmTaskSamplerTest,
                           GetConditionalSamplingProbability);
  FRIEND_TEST_ALL_PREFIXES(UkmTaskSamplerTest,
                           GetConditionalSamplingProbabilityWithEdgeCases);

  // Returns the conditional probability [0, 1] of
  // having to ukm sample given that has_thread_time has happened so that the
  // actual probability of sampling is |ukm_task_sampling_rate_|
  double GetConditionalSamplingProbability(bool has_thread_time);

  double thread_time_sampling_rate_;
  double ukm_task_sampling_rate_;

  std::mt19937_64 random_generator_;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_UKM_TASK_SAMPLER_H_
