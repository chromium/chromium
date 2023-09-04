// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/startup_helper.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool/initialization_util.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "content/common/thread_pool_util.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "content/public/common/content_switches.h"

namespace content {

std::unique_ptr<base::FieldTrialList> SetUpFieldTrialsAndFeatureList() {
  std::unique_ptr<base::FieldTrialList> field_trial_list;
  if (!base::FieldTrialList::GetInstance())
    field_trial_list = std::make_unique<base::FieldTrialList>();
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // Ensure any field trials specified on the command line are initialized.
  if (command_line->HasSwitch(::switches::kForceFieldTrials)) {
    // Create field trials without activating them, so that this behaves in a
    // consistent manner with field trials created from the server.
    bool result = base::FieldTrialList::CreateTrialsFromString(
        command_line->GetSwitchValueASCII(::switches::kForceFieldTrials));
    CHECK(result) << "Invalid --" << ::switches::kForceFieldTrials
                  << " list specified.";
  }

  base::FeatureList::InitializeInstance(
      command_line->GetSwitchValueASCII(switches::kEnableFeatures),
      command_line->GetSwitchValueASCII(switches::kDisableFeatures),
      GetSwitchDependentFeatureOverrides(*command_line));
  return field_trial_list;
}

namespace {

BASE_FEATURE(kBrowserThreadPoolAdjustment,
             "BrowserThreadPoolAdjustment",
             base::FEATURE_ENABLED_BY_DEFAULT);

// For iOS see ios/web/app/web_main_loop.cc.
constexpr base::FeatureParam<int> kBrowserThreadPoolMin {
  &kBrowserThreadPoolAdjustment, "BrowserThreadPoolMin",
#if BUILDFLAG(IS_ANDROID)
      6
#else
      16
#endif
};
constexpr base::FeatureParam<int> kBrowserThreadPoolMax {
  &kBrowserThreadPoolAdjustment, "BrowserThreadPoolMax",
#if BUILDFLAG(IS_ANDROID)
      8
#else
      32
#endif
};
constexpr base::FeatureParam<double> kBrowserThreadPoolCoresMultiplier{
    &kBrowserThreadPoolAdjustment, "BrowserThreadPoolCoresMultiplier", 0.6};
constexpr base::FeatureParam<int> kBrowserThreadPoolOffset{
    &kBrowserThreadPoolAdjustment, "BrowserThreadPoolOffset", 0};

}  // namespace

// TODO(scheduler-dev): Standardize thread pool logic and remove the need for
// specifying thread count manually.
void StartBrowserThreadPool() {
  // Ensure we always support at least one thread regardless of the field trial
  // param setting.
  auto min = static_cast<size_t>(std::max(kBrowserThreadPoolMin.Get(), 1));
  base::ThreadPoolInstance::InitParams thread_pool_init_params = {
      base::RecommendedMaxNumberOfThreadsInThreadGroup(
          min, kBrowserThreadPoolMax.Get(),
          kBrowserThreadPoolCoresMultiplier.Get(),
          kBrowserThreadPoolOffset.Get())};

#if BUILDFLAG(IS_WIN)
  thread_pool_init_params.common_thread_pool_environment = base::
      ThreadPoolInstance::InitParams::CommonThreadPoolEnvironment::COM_MTA;
#endif

  // If a renderer lives in the browser process, adjust the number of
  // threads in the foreground pool.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    thread_pool_init_params.max_num_foreground_threads =
        std::max(GetMinForegroundThreadsInRendererThreadPool(),
                 thread_pool_init_params.max_num_foreground_threads);
  }

  base::ThreadPoolInstance::Get()->Start(thread_pool_init_params);
}

}  // namespace content
