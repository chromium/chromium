// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/ukm_task_sampler.h"

#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {
namespace scheduler {

UkmTaskSampler::UkmTaskSampler(double thread_time_sampling_rate,
                               double ukm_task_sampling_rate)
    : thread_time_sampling_rate_(clampTo(thread_time_sampling_rate, 0.0, 1.0)),
      ukm_task_sampling_rate_(clampTo(ukm_task_sampling_rate, 0.0, 1.0)) {}

double UkmTaskSampler::GetConditionalSamplingProbability(bool has_thread_time) {
  if (thread_time_sampling_rate_ == 0.0 || ukm_task_sampling_rate_ == 0.0 ||
      !(ukm_task_sampling_rate_ < 1.0)) {
    return ukm_task_sampling_rate_;
  }

  if (thread_time_sampling_rate_ < ukm_task_sampling_rate_) {
    if (has_thread_time) {
      return 1.0;
    } else {
      // Note thread_time_sampling_rate_ < 1 given that
      // thread_time_sampling_rate_ < ukm_task_sampling_rate_ < 1
      return (ukm_task_sampling_rate_ - thread_time_sampling_rate_) /
             (1.0 - thread_time_sampling_rate_);
    }
  } else {
    if (has_thread_time) {
      // Also covers the case when ukm_task_sampling_rate_ ==
      // thread_time_sampling_rate_
      return ukm_task_sampling_rate_ / thread_time_sampling_rate_;
    } else {
      return 0.0;
    }
  }
}

bool UkmTaskSampler::ShouldRecordTaskUkm(bool has_thread_time) {
  double probability = GetConditionalSamplingProbability(has_thread_time);
  std::bernoulli_distribution dist(probability);
  return dist(random_generator_);
}

void UkmTaskSampler::SetUkmTaskSamplingRate(double rate) {
  ukm_task_sampling_rate_ = clampTo(rate, 0.0, 1.0);
}

}  // namespace scheduler
}  // namespace blink
