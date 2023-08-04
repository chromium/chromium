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
// (Only causes an effect when feature kNetworkServiceInProcess is disabled.)
BASE_FEATURE(kNetworkServiceSandbox,
             "NetworkServiceSandbox",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Enables a fine-grained seccomp-BPF syscall filter for the network service.
// Only has an effect if IsNetworkSandboxEnabled() returns true.
// If the network service sandbox is enabled and |kNetworkServiceSyscallFilter|
// is disabled, a seccomp-BPF filter will still be applied but it will not
// disallow any syscalls.
BASE_FEATURE(kNetworkServiceSyscallFilter,
             "NetworkServiceSyscallFilter",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Enables a fine-grained file path allowlist for the network service.
// Only has an effect if IsNetworkSandboxEnabled() returns true.
// If the network service sandbox is enabled and |kNetworkServiceFileAllowlist|
// is disabled, a file path allowlist will still be applied, but the policy will
// allow everything.
BASE_FEATURE(kNetworkServiceFileAllowlist,
             "NetworkServiceFileAllowlist",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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

// Enables very high job memory limits for sandboxed renderer processes. This
// sets a limit of 1Tb, effectively removing the Job memory limits, except in
// egregious cases.
BASE_FEATURE(kWinSboxHighRendererJobMemoryLimits,
             "WinSboxHighRendererJobMemoryLimits",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Emergency "off switch" for closing the KsecDD handle in cryptbase.dll just
// before sandbox lockdown in renderers.
BASE_FEATURE(kWinSboxRendererCloseKsecDD,
             "WinSboxRendererCloseKsecDD",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, only warm up `bcryptprimitives!ProcessPrng` - if disabled warms
// up `advapi32!RtlGenRandom`.
BASE_FEATURE(kWinSboxWarmupProcessPrng,
             "WinSboxWarmupProcessPrng",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, launch the network service within an LPAC sandbox. If disabled,
// the network service will run inside an App Container.
BASE_FEATURE(kWinSboxNetworkServiceSandboxIsLPAC,
             "WinSboxNetworkServiceSandboxIsLPAC",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
