// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/features.h"

#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace sandbox {
namespace policy {
namespace features {

#if !defined(OS_MAC) && !defined(OS_FUCHSIA)
// Enables network service sandbox.
// (Only causes an effect when feature kNetworkService is enabled.)
const base::Feature kNetworkServiceSandbox{"NetworkServiceSandbox",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // !defined(OS_MAC) && !defined(OS_FUCHSIA)

#if defined(OS_WIN)
// Emergency "off switch" for new Windows KTM security mitigation,
// sandbox::MITIGATION_KTM_COMPONENT.
const base::Feature kWinSboxDisableKtmComponent{
    "WinSboxDisableKtmComponent", base::FEATURE_ENABLED_BY_DEFAULT};

// Experiment for Windows sandbox security mitigation,
// sandbox::MITIGATION_EXTENSION_POINT_DISABLE.
const base::Feature kWinSboxDisableExtensionPoints{
    "WinSboxDisableExtensionPoint", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables GPU AppContainer sandbox on Windows.
const base::Feature kGpuAppContainer{"GpuAppContainer",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enables GPU Low Privilege AppContainer when combined with kGpuAppContainer.
const base::Feature kGpuLPAC{"GpuLPAC", base::FEATURE_ENABLED_BY_DEFAULT};

#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID)
// Controls whether the isolated XR service is sandboxed.
const base::Feature kXRSandbox{"XRSandbox", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Controls whether the Spectre variant 2 mitigation is enabled. We use a USE
// flag on some Chrome OS boards to disable the mitigation by disabling this
// feature in exchange for system performance.
const base::Feature kSpectreVariant2Mitigation{
    "SpectreVariant2Mitigation", base::FEATURE_ENABLED_BY_DEFAULT};

// An override for the Spectre variant 2 default behavior. Security sensitive
// users can enable this feature to ensure that the mitigation is always
// enabled.
const base::Feature kForceSpectreVariant2Mitigation{
    "ForceSpectreVariant2Mitigation", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
bool IsNetworkServiceSandboxLPACEnabled() {
  // Use LPAC for network sandbox instead of restricted token. Relies on
  // NetworkServiceSandbox being also enabled.
  const base::Feature kNetworkServiceSandboxLPAC{
      "NetworkServiceSandboxLPAC", base::FEATURE_DISABLED_BY_DEFAULT};

  // Since some APIs used for LPAC are unsupported below Windows 10, place a
  // check here in a central place.
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return false;

  return base::FeatureList::IsEnabled(kNetworkServiceSandbox) &&
         base::FeatureList::IsEnabled(kNetworkServiceSandboxLPAC);
}
#endif  // defined(OS_WIN)

}  // namespace features
}  // namespace policy
}  // namespace sandbox
