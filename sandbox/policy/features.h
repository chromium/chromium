// Copyright 2017 The Chromium Authors. All rights reserved.
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

namespace sandbox {
namespace policy {
namespace features {

#if !defined(OS_MAC) && !defined(OS_FUCHSIA)
SANDBOX_POLICY_EXPORT extern const base::Feature kNetworkServiceSandbox;
#endif

#if defined(OS_WIN)
SANDBOX_POLICY_EXPORT extern const base::Feature kWinSboxDisableKtmComponent;
SANDBOX_POLICY_EXPORT extern const base::Feature kWinSboxDisableExtensionPoints;
SANDBOX_POLICY_EXPORT extern const base::Feature kGpuAppContainer;
SANDBOX_POLICY_EXPORT extern const base::Feature kGpuLPAC;
#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID)
SANDBOX_POLICY_EXPORT extern const base::Feature kXRSandbox;
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
SANDBOX_POLICY_EXPORT extern const base::Feature kSpectreVariant2Mitigation;
SANDBOX_POLICY_EXPORT extern const base::Feature
    kForceSpectreVariant2Mitigation;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
// Returns whether the Network Service Sandbox is supported by the current
// Windows platform. Call this function rather than checking the
// kNetworkServiceSandbox feature directly.
SANDBOX_POLICY_EXPORT bool IsWinNetworkServiceSandboxSupported();
// Returns whether Windows Network Service Sandbox is enabled.
SANDBOX_POLICY_EXPORT bool IsWinNetworkServiceSandboxEnabled();
#endif
}  // namespace features
}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_FEATURES_H_
