// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/startup_helper.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/sys_info.h"
#include "base/task/task_scheduler/initialization_util.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "build/build_config.h"
#include "content/common/task_scheduler.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_switches.h"

namespace content {

namespace {

std::unique_ptr<base::TaskScheduler::InitParams>
GetDefaultTaskSchedulerInitParams() {
#if defined(OS_ANDROID)
  // Mobile config, for iOS see ios/web/app/web_main_loop.cc.
  return std::make_unique<base::TaskScheduler::InitParams>(
      base::SchedulerWorkerPoolParams(
          base::RecommendedMaxNumberOfThreadsInPool(2, 8, 0.1, 0),
          base::TimeDelta::FromSeconds(30)),
      base::SchedulerWorkerPoolParams(
          base::RecommendedMaxNumberOfThreadsInPool(2, 8, 0.1, 0),
          base::TimeDelta::FromSeconds(30)),
      base::SchedulerWorkerPoolParams(
          base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.3, 0),
          base::TimeDelta::FromSeconds(30)),
      base::SchedulerWorkerPoolParams(
          base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.3, 0),
          base::TimeDelta::FromSeconds(60)));
#else
  // Desktop config.
  return std::make_unique<base::TaskScheduler::InitParams>(
      base::SchedulerWorkerPoolParams(
          base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.1, 0),
          base::TimeDelta::FromSeconds(30)),
      base::SchedulerWorkerPoolParams(
          base::RecommendedMaxNumberOfThreadsInPool(3, 8, 0.1, 0),
          base::TimeDelta::FromSeconds(40)),
      base::SchedulerWorkerPoolParams(
          base::RecommendedMaxNumberOfThreadsInPool(8, 32, 0.3, 0),
          base::TimeDelta::FromSeconds(30)),
      base::SchedulerWorkerPoolParams(
          base::RecommendedMaxNumberOfThreadsInPool(8, 32, 0.3, 0),
          base::TimeDelta::FromSeconds(60))
#if defined(OS_WIN)
          ,
      base::TaskScheduler::InitParams::SharedWorkerPoolEnvironment::COM_MTA
#endif  // defined(OS_WIN)
      );
#endif
}

}  // namespace

std::unique_ptr<base::FieldTrialList> SetUpFieldTrialsAndFeatureList() {
  auto field_trial_list = std::make_unique<base::FieldTrialList>(nullptr);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // Ensure any field trials specified on the command line are initialized.
  if (command_line->HasSwitch(::switches::kForceFieldTrials)) {
    // Create field trials without activating them, so that this behaves in a
    // consistent manner with field trials created from the server.
    bool result = base::FieldTrialList::CreateTrialsFromString(
        command_line->GetSwitchValueASCII(::switches::kForceFieldTrials),
        std::set<std::string>());
    CHECK(result) << "Invalid --" << ::switches::kForceFieldTrials
                  << " list specified.";
  }

  base::FeatureList::InitializeInstance(
      command_line->GetSwitchValueASCII(switches::kEnableFeatures),
      command_line->GetSwitchValueASCII(switches::kDisableFeatures));
  return field_trial_list;
}

void StartBrowserTaskScheduler() {
  auto task_scheduler_init_params =
      GetContentClient()->browser()->GetTaskSchedulerInitParams();
  if (!task_scheduler_init_params)
    task_scheduler_init_params = GetDefaultTaskSchedulerInitParams();
  DCHECK(task_scheduler_init_params);

  // If a renderer lives in the browser process, adjust the number of
  // threads in the foreground pool.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    const base::SchedulerWorkerPoolParams&
        current_foreground_worker_pool_params(
            task_scheduler_init_params->foreground_worker_pool_params);
    task_scheduler_init_params->foreground_worker_pool_params =
        base::SchedulerWorkerPoolParams(
            std::max(GetMinThreadsInRendererTaskSchedulerForegroundPool(),
                     current_foreground_worker_pool_params.max_tasks()),
            current_foreground_worker_pool_params.suggested_reclaim_time(),
            current_foreground_worker_pool_params.backward_compatibility());
  }

  base::TaskScheduler::GetInstance()->Start(*task_scheduler_init_params.get());
}

}  // namespace content
