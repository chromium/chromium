// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/features.h"

#include "build/build_config.h"

namespace service_manager {
namespace features {

// Enables audio service sandbox.
// (Only causes an effect when feature kAudioServiceOutOfProcess is enabled.)
const base::Feature kAudioServiceSandbox {
  "AudioServiceSandbox",
#if defined(OS_WIN) || defined(OS_MACOSX)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
};

// Enables network service sandbox.
// (Only causes an effect when feature kNetworkService is enabled.)
const base::Feature kNetworkServiceSandbox{"NetworkServiceSandbox",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_WIN)
// Emergency "off switch" for new Windows sandbox security mitigation,
// sandbox::MITIGATION_EXTENSION_POINT_DISABLE.
const base::Feature kWinSboxDisableExtensionPoints{
    "WinSboxDisableExtensionPoint", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID)
// Controls whether the isolated XR service is sandboxed.
const base::Feature kXRSandbox{"XRSandbox", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // !defined(OS_ANDROID)

}  // namespace features
}  // namespace service_manager
