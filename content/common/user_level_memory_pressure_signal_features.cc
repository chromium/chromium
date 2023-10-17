// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/user_level_memory_pressure_signal_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)

namespace content::features {

namespace {

constexpr base::TimeDelta kDefaultMinimumInterval = base::Minutes(10);

// Each renderer does not generate memory pressure signals until the interval
// has passed after page loading is finished. This parameter must be larger
// than or equal to the time from navigation start to the time the
// DOMContentLoaded event is finished. 5min is much larger than
// the 99p of PageLoad.DocumentTiming.NavigationToDOMContentLoadedEventFired
// (14sec) and we expect the DOMContentLoaded events will finish in 5min.
// Negative inert interval disables delayed memory pressure signals
// This is intended to keep the old behavior.
constexpr base::TimeDelta kDefaultInertInterval = base::Minutes(5);

}  // namespace

// Monitor total private memory footprint and dispatch memory pressure signal
// if the value exceeds the pre-defined threshold.

// (for Android 3GB devices)
BASE_FEATURE(kUserLevelMemoryPressureSignalOn3GbDevices,
             "UserLevelMemoryPressureSignalOn3GbDevices",
             base::FEATURE_DISABLED_BY_DEFAULT);
// (for Android 4GB devices)
BASE_FEATURE(kUserLevelMemoryPressureSignalOn4GbDevices,
             "UserLevelMemoryPressureSignalOn4GbDevices",
             base::FEATURE_ENABLED_BY_DEFAULT);
// (for Android 6GB devices)
BASE_FEATURE(kUserLevelMemoryPressureSignalOn6GbDevices,
             "UserLevelMemoryPressureSignalOn6GbDevices",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsUserLevelMemoryPressureSignalEnabledOn3GbDevices() {
  static bool s_enabled =
      base::SysInfo::Is3GbDevice() &&
      base::FeatureList::IsEnabled(kUserLevelMemoryPressureSignalOn3GbDevices);
  return s_enabled;
}

bool IsUserLevelMemoryPressureSignalEnabledOn4GbDevices() {
  // Because of Android carveouts, AmountOfPhysicalMemory() returns smaller
  // than the actual memory size, So we will use a small lowerbound than 4GB
  // to discriminate real 4GB devices from lower memory ones.
  static bool s_enabled =
      base::SysInfo::Is4GbDevice() &&
      base::FeatureList::IsEnabled(kUserLevelMemoryPressureSignalOn4GbDevices);
  return s_enabled;
}

bool IsUserLevelMemoryPressureSignalEnabledOn6GbDevices() {
  static bool s_enabled =
      base::SysInfo::Is6GbDevice() &&
      base::FeatureList::IsEnabled(kUserLevelMemoryPressureSignalOn6GbDevices);
  return s_enabled;
}

// Minimum time interval between generated memory pressure signals.
base::TimeDelta MinUserMemoryPressureIntervalOn3GbDevices() {
  static const base::FeatureParam<base::TimeDelta> kMinimumInterval{
      &kUserLevelMemoryPressureSignalOn3GbDevices, "minimum_interval",
      kDefaultMinimumInterval};
  return kMinimumInterval.Get();
}

base::TimeDelta MinUserMemoryPressureIntervalOn4GbDevices() {
  static const base::FeatureParam<base::TimeDelta> kMinimumInterval{
      &kUserLevelMemoryPressureSignalOn4GbDevices, "minimum_interval",
      kDefaultMinimumInterval};
  return kMinimumInterval.Get();
}

base::TimeDelta MinUserMemoryPressureIntervalOn6GbDevices() {
  static const base::FeatureParam<base::TimeDelta> kMinimumInterval{
      &kUserLevelMemoryPressureSignalOn6GbDevices, "minimum_interval",
      kDefaultMinimumInterval};
  return kMinimumInterval.Get();
}

base::TimeDelta InertIntervalFor3GbDevices() {
  static const base::FeatureParam<base::TimeDelta> kInertInterval{
      &features::kUserLevelMemoryPressureSignalOn3GbDevices,
      "inert_interval_after_loading", kDefaultInertInterval};
  return kInertInterval.Get();
}

base::TimeDelta InertIntervalFor4GbDevices() {
  static const base::FeatureParam<base::TimeDelta> kInertInterval{
      &features::kUserLevelMemoryPressureSignalOn4GbDevices,
      "inert_interval_after_loading", kDefaultInertInterval};
  return kInertInterval.Get();
}

base::TimeDelta InertIntervalFor6GbDevices() {
  static const base::FeatureParam<base::TimeDelta> kInertInterval{
      &features::kUserLevelMemoryPressureSignalOn6GbDevices,
      "inert_interval_after_loading", kDefaultInertInterval};
  return kInertInterval.Get();
}

}  // namespace content::features

#endif  // BUILDFLAG(IS_ANDROID)
