// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_ANDROID_OPENXR_PLATFORM_HELPER_ANDROID_H_
#define COMPONENTS_WEBXR_ANDROID_OPENXR_PLATFORM_HELPER_ANDROID_H_

#include "device/vr/openxr/openxr_platform_helper.h"

#include "base/android/scoped_java_ref.h"
#include "device/vr/openxr/openxr_platform.h"

namespace webxr {

// Android specific implementation of the OpenXrPlatformHelper.
class OpenXrPlatformHelperAndroid : public device::OpenXrPlatformHelper {
 public:
  OpenXrPlatformHelperAndroid();
  ~OpenXrPlatformHelperAndroid() override;

  // OpenXrPlatformHelper
  std::unique_ptr<device::OpenXrGraphicsBinding> GetGraphicsBinding() override;
  const void* GetPlatformCreateInfo(
      const device::OpenXrCreateInfo& create_info) override;
  device::mojom::XRDeviceData GetXRDeviceData() override;
  bool Initialize() override;

 private:
  XrInstanceCreateInfoAndroidKHR create_info_{
      XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
  base::android::ScopedJavaGlobalRef<jobject> activity_;
  base::android::ScopedJavaGlobalRef<jobject> app_context_;
};

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_ANDROID_OPENXR_PLATFORM_HELPER_ANDROID_H_
