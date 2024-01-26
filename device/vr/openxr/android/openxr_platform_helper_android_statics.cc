// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/openxr/openxr_platform_helper.h"

#include <vector>

#include "device/vr/openxr/openxr_platform.h"
#include "third_party/openxr/dev/xr_android.h"

// The actual `OpenXrPlatformHelperAndroid` is implemented in //components/webxr
// however, in //device/vr, we must provide an implementation for the loose
// static methods associated with the OpenXrPlatformHelper based upon the
// compiled in platform-methods; so we define those statics here in this file.
namespace device {
// static
void OpenXrPlatformHelper::GetRequiredExtensions(
    std::vector<const char*>& extensions) {
  extensions.push_back(XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME);
}

// static
std::vector<const char*> OpenXrPlatformHelper::GetOptionalExtensions() {
  return {};
}

}  // namespace device
