// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the content
// module.

#ifndef SANDBOX_POLICY_FEATURES_H_
#define SANDBOX_POLICY_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "sandbox/policy/export.h"

namespace sandbox::policy::features {

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_FUCHSIA)
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kNetworkServiceSandbox);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kNetworkServiceSyscallFilter);
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kNetworkServiceFileAllowlist);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#endif

#if BUILDFLAG(IS_WIN)
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kWinSboxDisableExtensionPoints);
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kGpuAppContainer);
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kGpuLPAC);
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kPrintCompositorLPAC);
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kRendererAppContainer);
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kWinSboxNetworkServiceSandboxIsLPAC);
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kWinSboxForceRendererCodeIntegrity);
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kWinSboxZeroAppShim);
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kNetworkServiceCodeIntegrity);
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kWinSboxNoFakeGdiInit);
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(
    kWinSboxRestrictCoreSharingOnRenderer);
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kWinSboxParallelProcessLaunch);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kSpectreVariant2Mitigation);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(
    kForceDisableSpectreVariant2MitigationInNetworkService);

SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kHigherRendererMemoryLimit);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kCacheMacSandboxProfiles);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_ANDROID)
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kUseRendererProcessPolicy);
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kRestrictRendererPoliciesInBaseline);
SANDBOX_POLICY_EXPORT BASE_DECLARE_FEATURE(kRestrictCloneParameters);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
// Returns whether the network sandbox is supported. This is different from
// IsAppContainerSandboxSupported as the Network Service uses some newer APIs to
// correctly function when sandboxed.
SANDBOX_POLICY_EXPORT bool IsNetworkSandboxSupported();
#endif  // BUILDFLAG(IS_WIN)

// Returns whether the network sandbox is enabled for the current platform
// configuration. This might be overridden by the content embedder so prefer
// calling ContentBrowserClient::ShouldSandboxNetworkService().
SANDBOX_POLICY_EXPORT bool IsNetworkSandboxEnabled();

#if BUILDFLAG(IS_WIN)
// Returns whether parallel launching is enabled.
SANDBOX_POLICY_EXPORT bool IsParallelLaunchEnabled();
#endif  // BUILDFLAG(IS_WIN)
}  // namespace sandbox::policy::features

#endif  // SANDBOX_POLICY_FEATURES_H_
