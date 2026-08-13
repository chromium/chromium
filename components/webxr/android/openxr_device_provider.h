// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_ANDROID_OPENXR_DEVICE_PROVIDER_H_
#define COMPONENTS_WEBXR_ANDROID_OPENXR_DEVICE_PROVIDER_H_

#include <memory>

#include "components/webxr/android/openxr_platform_helper_android.h"
#include "device/vr/openxr/context_provider_callbacks.h"
#include "device/vr/openxr/openxr_device.h"
#include "device/vr/public/cpp/vr_device_provider.h"

namespace device {
class OpenXrDevice;
}
namespace webxr {

class OpenXrDeviceProvider : public device::VRDeviceProvider {
 public:
  OpenXrDeviceProvider();
  ~OpenXrDeviceProvider() override;

  OpenXrDeviceProvider(const OpenXrDeviceProvider&) = delete;
  OpenXrDeviceProvider& operator=(const OpenXrDeviceProvider&) = delete;

  void Initialize(device::VRDeviceProviderClient* client,
                  content::WebContents* initializing_web_contents) override;
  bool Initialized() override;

 private:
  void CreateContextProviderAsync(
      VizContextProviderCallback viz_context_provider_callback);

  // Must outlive `openxr_device_`
  std::unique_ptr<OpenXrPlatformHelperAndroid> openxr_platform_helper_;
  std::unique_ptr<device::OpenXrDevice> openxr_device_;
  bool initialized_ = false;
};

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_ANDROID_OPENXR_DEVICE_PROVIDER_H_
