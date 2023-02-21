// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/features.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "sandbox/features.h"

namespace sandbox::policy::features {

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
// Enables network service sandbox.
// (Only causes an effect when feature kNetworkService is enabled.)
BASE_FEATURE(kNetworkServiceSandbox,
             "NetworkServiceSandbox",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN)
// Experiment for Windows sandbox security mitigation,
// sandbox::MITIGATION_EXTENSION_POINT_DISABLE.
BASE_FEATURE(kWinSboxDisableExtensionPoints,
             "WinSboxDisableExtensionPoint",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables GPU AppContainer sandbox on Windows.
BASE_FEATURE(kGpuAppContainer,
             "GpuAppContainer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables GPU Low Privilege AppContainer when combined with kGpuAppContainer.
BASE_FEATURE(kGpuLPAC,
             "GpuLPAC",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Renderer AppContainer
BASE_FEATURE(kRendererAppContainer,
             "RendererAppContainer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables shared/fixed policy for Windows sandbox policies.
BASE_FEATURE(kSharedSandboxPolicies,
             "SharedSandboxPolicies",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Emergency "off switch" for renderer environment filtering, this feature can
// be removed around the M113 timeline. See https://crbug.com/1403087.
BASE_FEATURE(kRendererFilterEnvironment,
             "RendererFilterEnvironment",
             base::FEATURE_ENABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Controls whether the Spectre variant 2 mitigation is enabled. We use a USE
// flag on some Chrome OS boards to disable the mitigation by disabling this
// feature in exchange for system performance.
BASE_FEATURE(kSpectreVariant2Mitigation,
             "SpectreVariant2Mitigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// An override for the Spectre variant 2 default behavior. Security sensitive
// users can enable this feature to ensure that the mitigation is always
// enabled.
BASE_FEATURE(kForceSpectreVariant2Mitigation,
             "ForceSpectreVariant2Mitigation",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
// Enables caching compiled sandbox profiles. Only some profiles support this,
// as controlled by CanCacheSandboxPolicy().
BASE_FEATURE(kCacheMacSandboxProfiles,
             "CacheMacSandboxProfiles",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

bool IsNetworkSandboxEnabled() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
  return true;
#else
#if BUILDFLAG(IS_WIN)
  if (!sandbox::features::IsAppContainerSandboxSupported())
    return false;
#endif  // BUILDFLAG(IS_WIN)
  // Check feature status.
  return base::FeatureList::IsEnabled(kNetworkServiceSandbox);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
}

}  // namespace sandbox::policy::features
