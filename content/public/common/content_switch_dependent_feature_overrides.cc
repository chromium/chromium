// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_switch_dependent_feature_overrides.h"

#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "net/base/features.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"

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
      {switches::kAppCacheForceEnabled,
       std::cref(blink::features::kAppCacheRequireOriginTrial),
       base::FeatureList::OVERRIDE_DISABLE_FEATURE},
      // Overrides for --enable-experimental-web-platform-features.
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kCookieDeprecationMessages),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(network::features::kCrossOriginOpenerPolicyReporting),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(network::features::kCrossOriginOpenerPolicyAccessReporting),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(network::features::kCrossOriginIsolated),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(network::features::kCrossOriginEmbedderPolicy),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kDocumentPolicyNegotiation),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kFeaturePolicyForClientHints),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kLangClientHintHeader),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kUserAgentClientHint),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kOriginPolicy),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kOriginIsolationHeader),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kEnableNewCanvas2DAPI),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kCriticalClientHint),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(net::features::kSchemefulSameSite),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},

      // Overrides for --enable-experimental-cookie-features.
      {switches::kEnableExperimentalCookieFeatures,
       std::cref(features::kCookieDeprecationMessages),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalCookieFeatures,
       std::cref(net::features::kCookiesWithoutSameSiteMustBeSecure),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalCookieFeatures,
       std::cref(net::features::kSameSiteByDefaultCookies),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalCookieFeatures,
       std::cref(net::features::kSameSiteDefaultChecksMethodRigorously),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalCookieFeatures,
       std::cref(net::features::kSchemefulSameSite),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
  };

  std::vector<base::FeatureList::FeatureOverrideInfo> overrides;
  for (const auto& info : override_info) {
    if (command_line.HasSwitch(info.switch_name))
      overrides.emplace_back(std::make_pair(info.feature, info.override_state));
  }
  return overrides;
}

}  // namespace content
