// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/field_trial.h"

#include <tuple>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_shared_memory.h"
#include "build/build_config.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "content/public/common/content_switches.h"

namespace content {

void InitializeFieldTrialAndFeatureList() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  base::HistogramSharedMemory::InitFromLaunchParameters(command_line);

  // Initialize statistical testing infrastructure.
  //
  // This is intentionally leaked since it needs to live for the duration of the
  // process and there's no benefit in cleaning it up at exit.
  base::FieldTrialList* leaked_field_trial_list = new base::FieldTrialList();
  ANNOTATE_LEAKING_OBJECT_PTR(leaked_field_trial_list);
  std::ignore = leaked_field_trial_list;

  // Ensure any field trials in browser are reflected into the child process.
  base::FieldTrialList::CreateTrialsInChildProcess(command_line);
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  base::FieldTrialList::ApplyFeatureOverridesInChildProcess(feature_list.get());
  // TODO(crbug.com/40638048): This may be redundant. The way this is supposed
  // to work is that the parent process's state should be passed via
  // command-line to the child process, such that a feature explicitly enabled
  // or disabled in the parent process via this mechanism (since the browser
  // process also registers these switch-dependent overrides), it will get
  // passed via the command line - so then no extra logic would be needed in the
  // child.
  // TODO(chlily): Test this more thoroughly and understand the behavior to see
  // whether this is actually needed.
  feature_list->RegisterExtraFeatureOverrides(
      GetSwitchDependentFeatureOverrides(command_line));
  base::FeatureList::SetInstance(std::move(feature_list));
}

}  // namespace content
