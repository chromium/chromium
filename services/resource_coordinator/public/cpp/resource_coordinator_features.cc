// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"

namespace {

constexpr char kMainThreadTaskLoadLowThresholdParameterName[] =
    "mainThreadTaskLoadLowThreshold";

}  // namespace

namespace features {

const base::Feature kPageAlmostIdle{"PageAlmostIdle",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables CPU/memory performance measurements on PageAlmostIdle events.
const base::Feature kPerformanceMeasurement{"PerformanceMeasurement",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

namespace resource_coordinator {

bool IsPageAlmostIdleSignalEnabled() {
  return base::FeatureList::IsEnabled(features::kPageAlmostIdle);
}

int GetMainThreadTaskLoadLowThreshold() {
  static const int kDefaultThreshold = 25;

  std::string value_str = base::GetFieldTrialParamValueByFeature(
      features::kPageAlmostIdle, kMainThreadTaskLoadLowThresholdParameterName);
  int main_thread_task_load_low_threshold;
  if (value_str.empty() ||
      !base::StringToInt(value_str, &main_thread_task_load_low_threshold)) {
    return kDefaultThreshold;
  }
  return main_thread_task_load_low_threshold;
}

}  // namespace resource_coordinator
