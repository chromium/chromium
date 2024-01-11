// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/network_service_util_internal.h"

#include "base/check.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "content/browser/network/network_service_util_internal.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#endif

namespace content {
namespace {

std::optional<bool> g_force_network_service_process_in_or_out;

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kNetworkServiceOutOfProcessMemoryThreshold,
             "NetworkServiceOutOfProcessMemoryThreshold",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Using 1077 rather than 1024 because it helps ensure that devices with
// exactly 1GB of RAM won't get included because of inaccuracies or off-by-one
// errors.
constexpr base::FeatureParam<int> kNetworkServiceOutOfProcessThresholdMb{
    &kNetworkServiceOutOfProcessMemoryThreshold,
    "network_service_oop_threshold_mb", 1077};
#endif

}  // namespace

void ForceInProcessNetworkServiceImpl() {
  CHECK(!g_force_network_service_process_in_or_out ||
        *g_force_network_service_process_in_or_out);
  g_force_network_service_process_in_or_out = true;
}
void ForceOutOfProcessNetworkServiceImpl() {
  CHECK(!g_force_network_service_process_in_or_out ||
        !*g_force_network_service_process_in_or_out);
  g_force_network_service_process_in_or_out = false;
}

bool IsInProcessNetworkServiceImpl() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    return true;
  }

  if (g_force_network_service_process_in_or_out) {
    return *g_force_network_service_process_in_or_out;
  }

#if BUILDFLAG(IS_ANDROID)
  // Check RAM size before looking at kNetworkServiceInProcess flag
  // so that we can throttle the finch groups including control.
  if (base::SysInfo::AmountOfPhysicalMemoryMB() <=
      kNetworkServiceOutOfProcessThresholdMb.Get()) {
    return true;
  }
#endif

  return base::FeatureList::IsEnabled(features::kNetworkServiceInProcess);
}

}  // namespace content
