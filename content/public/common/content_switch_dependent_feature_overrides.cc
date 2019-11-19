// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_switch_dependent_feature_overrides.h"

#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

namespace content {

std::vector<base::FeatureList::FeatureOverrideInfo>
GetSwitchDependentFeatureOverrides(const base::CommandLine& command_line) {
  // Describes a switch-dependent override.
  struct SwitchDependentFeatureOverrideInfo {
    // Switch that the override depends upon. The override will be registered if
    // this switch is present.
    const char* switch_name;
    // Feature to override.
    const std::reference_wrapper<const base::Feature> feature;
    // State to override the feature with.
    base::FeatureList::OverrideState override_state;
  } override_info[] = {
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kCookieDeprecationMessages),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
  };

  // TODO(chlily): There are currently a few places where, to check if some
  // functionality should be enabled, we check base::FeatureList::IsEnabled on
  // some base::Feature and then also check whether the CommandLine for the
  // current process has the switch kEnableExperimentalWebPlatformFeatures. It
  // would be nice to have those features get set up here as switch-dependent
  // feature overrides. That way, we could eliminate directly checking the
  // command line for --enable-experimental-web-platform-features, and would
  // have the base::Feature corresponding to that functionality correctly
  // reflect whether it should be enabled.

  std::vector<base::FeatureList::FeatureOverrideInfo> overrides;
  for (const auto& info : override_info) {
    if (command_line.HasSwitch(info.switch_name))
      overrides.emplace_back(std::make_pair(info.feature, info.override_state));
  }
  return overrides;
}

}  // namespace content
