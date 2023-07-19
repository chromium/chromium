// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_ANDROID_H_
#define SERVICES_DEVICE_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_ANDROID_H_

#include "services/device/device_posture/device_posture_platform_provider.h"

namespace device {

class DevicePosturePlatformProviderAndroid
    : public DevicePosturePlatformProvider {
 public:
  DevicePosturePlatformProviderAndroid();
  ~DevicePosturePlatformProviderAndroid() override;

  DevicePosturePlatformProviderAndroid(
      const DevicePosturePlatformProviderAndroid&) = delete;
  DevicePosturePlatformProviderAndroid& operator=(
      const DevicePosturePlatformProviderAndroid&) = delete;

  device::mojom::DevicePostureType GetDevicePosture() override;
  const std::vector<gfx::Rect>& GetViewportSegments() override;
  void StartListening() override;
  void StopListening() override;

 private:
  std::vector<gfx::Rect> current_viewport_segments_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_ANDROID_H_
