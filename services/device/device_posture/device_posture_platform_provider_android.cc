// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_posture/device_posture_platform_provider_android.h"

namespace device {

DevicePosturePlatformProviderAndroid::DevicePosturePlatformProviderAndroid() =
    default;

DevicePosturePlatformProviderAndroid::~DevicePosturePlatformProviderAndroid() =
    default;

device::mojom::DevicePostureType
DevicePosturePlatformProviderAndroid::GetDevicePosture() {
  return device::mojom::DevicePostureType::kContinuous;
}
const std::vector<gfx::Rect>&
DevicePosturePlatformProviderAndroid::GetViewportSegments() {
  return current_viewport_segments_;
}

void DevicePosturePlatformProviderAndroid::StartListening() {}

void DevicePosturePlatformProviderAndroid::StopListening() {}

}  // namespace device
