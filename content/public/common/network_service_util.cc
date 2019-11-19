// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/network_service_util.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

#if defined(OS_ANDROID)
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#endif

namespace content {
namespace {

#if defined(OS_ANDROID)
const base::Feature kNetworkServiceOutOfProcessMemoryThreshold{
    "NetworkServiceOutOfProcessMemoryThreshold",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Using 1077 rather than 1024 because 1) it helps ensure that devices with
// exactly 1GB of RAM won't get included because of inaccuracies or off-by-one
// errors and 2) this is the bucket boundary in Memory.Stats.Win.TotalPhys2.
constexpr base::FeatureParam<int> kNetworkServiceOutOfProcessThresholdMb{
    &kNetworkServiceOutOfProcessMemoryThreshold,
    "network_service_oop_threshold_mb", 1077};
#endif

// Indicates whether the network service is forced to be running in the browser
// process.
bool g_force_in_process_network_service = false;

}  // namespace

bool IsOutOfProcessNetworkService() {
  return !IsInProcessNetworkService();
}

bool IsInProcessNetworkService() {
  if (g_force_in_process_network_service ||
      base::FeatureList::IsEnabled(features::kNetworkServiceInProcess) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    return true;
  }

#if defined(OS_ANDROID)
  return base::SysInfo::AmountOfPhysicalMemoryMB() <=
         kNetworkServiceOutOfProcessThresholdMb.Get();
#endif
  return false;
}

void ForceInProcessNetworkService(bool is_forced) {
  g_force_in_process_network_service = is_forced;
}

}  // namespace content
