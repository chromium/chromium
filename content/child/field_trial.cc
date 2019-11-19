// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/field_trial.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "content/public/common/content_switches.h"
#include "services/service_manager/embedder/descriptors.h"

namespace content {

void InitializeFieldTrialAndFeatureList() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Initialize statistical testing infrastructure.  We set the entropy
  // provider to nullptr to disallow non-browser processes from creating
  // their own one-time randomized trials; they should be created in the
  // browser process.
  //
  // This is intentionally leaked since it needs to live for the duration of the
  // process and there's no benefit in cleaning it up at exit.
  base::FieldTrialList* leaked_field_trial_list =
      new base::FieldTrialList(nullptr);
  ANNOTATE_LEAKING_OBJECT_PTR(leaked_field_trial_list);
  ignore_result(leaked_field_trial_list);

// Ensure any field trials in browser are reflected into the child
// process.
#if defined(OS_WIN) || defined(OS_MACOSX)
  base::FieldTrialList::CreateTrialsFromCommandLine(
      command_line, switches::kFieldTrialHandle, -1);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  // On POSIX systems that use the zygote, we get the trials from a shared
  // memory segment backed by an fd instead of the command line.
  base::FieldTrialList::CreateTrialsFromCommandLine(
      command_line, switches::kFieldTrialHandle,
      service_manager::kFieldTrialDescriptor);
#endif

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  base::FieldTrialList::CreateFeaturesFromCommandLine(
      command_line, switches::kEnableFeatures, switches::kDisableFeatures,
      feature_list.get());
  // TODO(crbug.com/988603): This may be redundant. The way this is supposed to
  // work is that the parent process's state should be passed via command-line
  // to the child process, such that a feature explicitly enabled or disabled in
  // the parent process via this mechanism (since the browser process also
  // registers these switch-dependent overrides), it will get passed via the
  // command line - so then no extra logic would be needed in the child.
  // TODO(chlily): Test this more thoroughly and understand the behavior to see
  // whether this is actually needed.
  feature_list->RegisterExtraFeatureOverrides(
      GetSwitchDependentFeatureOverrides(command_line));
  base::FeatureList::SetInstance(std::move(feature_list));
}

}  // namespace content
