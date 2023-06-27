// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_ANDROID_OPENXR_PLATFORM_HELPER_ANDROID_H_
#define COMPONENTS_WEBXR_ANDROID_OPENXR_PLATFORM_HELPER_ANDROID_H_

#include "device/vr/openxr/openxr_platform_helper.h"

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "components/webxr/android/xr_session_coordinator.h"
#include "device/vr/openxr/openxr_platform.h"

namespace webxr {

// Android specific implementation of the OpenXrPlatformHelper.
class OpenXrPlatformHelperAndroid : public device::OpenXrPlatformHelper {
 public:
  OpenXrPlatformHelperAndroid();
  ~OpenXrPlatformHelperAndroid() override;

  // OpenXrPlatformHelper
  std::unique_ptr<device::OpenXrGraphicsBinding> GetGraphicsBinding() override;
  void GetPlatformCreateInfo(const device::OpenXrCreateInfo& create_info,
                             PlatformCreateInfoReadyCallback) override;
  device::mojom::XRDeviceData GetXRDeviceData() override;
  bool Initialize() override;

  XrResult DestroyInstance(XrInstance& instance) override;

 private:
  std::unique_ptr<XrSessionCoordinator> session_coordinator_;

  XrInstanceCreateInfoAndroidKHR create_info_{
      XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
  base::android::ScopedJavaGlobalRef<jobject> activity_;
  base::android::ScopedJavaGlobalRef<jobject> app_context_;

  void OnXrActivityReady(PlatformCreateInfoReadyCallback callback,
                         const base::android::JavaParamRef<jobject>& activity);
};

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_ANDROID_OPENXR_PLATFORM_HELPER_ANDROID_H_
