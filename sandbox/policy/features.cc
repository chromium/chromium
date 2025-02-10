// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "sandbox/features.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

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

// Enables Print Compositor Low Privilege AppContainer. Note, this might be
// overridden and disabled by policy.
BASE_FEATURE(kPrintCompositorLPAC,
             "PrintCompositorLPAC",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Renderer AppContainer
BASE_FEATURE(kRendererAppContainer,
             "RendererAppContainer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, launch the network service within an LPAC sandbox. If disabled,
// the network service will run inside an App Container.
BASE_FEATURE(kWinSboxNetworkServiceSandboxIsLPAC,
             "WinSboxNetworkServiceSandboxIsLPAC",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, always launch the renderer process with Code Integrity Guard
// enabled, regardless of the local policy configuration. If disabled, then
// policy is respected. This acts as an emergency "off switch" for the
// deprecation of the RendererCodeIntegrityEnabled policy.
BASE_FEATURE(kWinSboxForceRendererCodeIntegrity,
             "WinSboxForceRendererCodeIntegrity",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, modifies the child's PEB to stop further application of
// appcompat in the child. Does not affect the browser or unsandboxed
// processes. The feature has no effect for WOW (32bit on 64bit) installs.
BASE_FEATURE(kWinSboxZeroAppShim,
             "WinSboxZeroAppShim",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables pre-launch Code Integrity Guard (CIG) for Chrome network service
// process, when running on Windows 10 1511 and above. This has no effect if
// NetworkServiceSandbox feature is disabled, or if using a component or ASAN
// build. See https://blogs.windows.com/blog/tag/code-integrity-guard/.
BASE_FEATURE(kNetworkServiceCodeIntegrity,
             "NetworkServiceCodeIntegrity",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Run win32k lockdown without applying the interceptions to fake out the
// dllmain of gdi32 and user32. With this feature enabled, processes with
// win32k lockdown policy will fail to load gdi32.dll and user32.dll.
// TODO(crbug.com/326277735) this feature is under development and not
// completely supported in every process type, may cause delayload failures.
BASE_FEATURE(kWinSboxNoFakeGdiInit,
             "WinSboxNoFakeGdiInit",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables Restrict Core Sharing mitigation for the renderer process, when
// running Windows 11 Build 26100 (24H2) and above. See param definition of
// RestrictCoreSharing in
// https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-
// process_mitigation_side_channel_isolation_policy
BASE_FEATURE(kWinSboxRestrictCoreSharingOnRenderer,
             "WinSboxRestrictCoreSharingOnRenderer",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables parallel process launching using the thread pool. Flag retained
// as a kill-switch.
BASE_FEATURE(kWinSboxParallelProcessLaunch,
             "WinSboxParallelProcessLaunch",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables Csrss lockdown in supported processes by closing all ALPC
// ports before sandbox lockdown. See crbug.com/40408399 for details.
BASE_FEATURE(kEnableCsrssLockdown,
             "EnableCsrssLockdown",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Filters most environment variables out for kService and kServiceWithJit
// sandboxed processes. Flag retained as a kill-switch.
BASE_FEATURE(kWinSboxFilterServiceEnvironment,
             "WinSboxFilterServiceEnvironment",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
// Controls whether the Spectre variant 2 mitigation is enabled. We use a USE
// flag on some Chrome OS boards to disable the mitigation by disabling this
// feature in exchange for system performance.
BASE_FEATURE(kSpectreVariant2Mitigation,
             "SpectreVariant2Mitigation",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Increase the renderer sandbox memory limit. As of 2023, there are no limits
// on macOS, and a 1TiB limit on Windows. There are reports of users bumping
// into the limit. This increases the limit by 2x compared to the default
// state. We are not increasing it all the way as on Windows as Linux systems
// typically ship with overcommit, so there is no "commit limit" to save us
// from egregious cases as on Windows.
BASE_FEATURE(kHigherRendererMemoryLimit,
             "HigherRendererMemoryLimit",
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
// Enables caching compiled sandbox profiles. Only some profiles support this,
// as controlled by CanCacheSandboxPolicy().
BASE_FEATURE(kCacheMacSandboxProfiles,
             "CacheMacSandboxProfiles",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_ANDROID)
// Enables the renderer on Android to use a separate seccomp policy.
BASE_FEATURE(kUseRendererProcessPolicy,
             "UseRendererProcessPolicy",
             base::FEATURE_ENABLED_BY_DEFAULT);
// When enabled, this features restricts a set of syscalls in
// BaselinePolicyAndroid that are used by RendererProcessPolicy.
BASE_FEATURE(kRestrictRendererPoliciesInBaseline,
             "RestrictRendererPoliciesInBaseline",
             base::FEATURE_ENABLED_BY_DEFAULT);
// When enabled, restrict clone to just flags used by fork and pthread_create on
// android.
BASE_FEATURE(kRestrictCloneParameters,
             "RestrictCloneParameters",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
bool IsNetworkSandboxSupported() {
  // Temporary fix to avoid using network sandbox on ARM64 until root cause for
  // https://crbug.com/40223285 is diagnosed.
  if (base::win::OSInfo::GetInstance()->GetArchitecture() ==
          base::win::OSInfo::ARM64_ARCHITECTURE ||
      base::win::OSInfo::GetInstance()->IsWowX86OnARM64() ||
      base::win::OSInfo::GetInstance()->IsWowAMD64OnARM64()) {
    return false;
  }

  // Network service sandbox uses GetNetworkConnectivityHint which is only
  // supported on Windows 10 Build 19041 (20H1) so versions before that wouldn't
  // have a working network change notifier when running in the sandbox.
  // TODO(crbug.com/40915451): Move this to an API that works earlier than 20H1
  // and also works in the LPAC sandbox.
  static const bool supported =
      base::win::GetVersion() >= base::win::Version::WIN10_20H1;
  if (!supported) {
    return false;
  }

  // App container must be already supported on 20H1, but double check it here.
  CHECK(sandbox::features::IsAppContainerSandboxSupported());

  return true;
}
#endif  // BUILDFLAG(IS_WIN)

bool IsNetworkSandboxEnabled() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
  return true;
#else
#if BUILDFLAG(IS_WIN)
  if (!IsNetworkSandboxSupported()) {
    return false;
  }
#endif  // BUILDFLAG(IS_WIN)
  // Check feature status.
  return base::FeatureList::IsEnabled(kNetworkServiceSandbox);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)
}

#if BUILDFLAG(IS_WIN)
bool IsParallelLaunchEnabled() {
  return base::FeatureList::IsEnabled(kWinSboxParallelProcessLaunch);
}
#endif  // BUILDFLAG(IS_WIN)
}  // namespace sandbox::policy::features
