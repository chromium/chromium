// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_posture/device_posture_platform_provider_default.h"

namespace device {

DevicePosturePlatformProviderDefault::DevicePosturePlatformProviderDefault() =
    default;

DevicePosturePlatformProviderDefault::~DevicePosturePlatformProviderDefault() =
    default;

device::mojom::DevicePostureType
DevicePosturePlatformProviderDefault::GetDevicePosture() {
  return device::mojom::DevicePostureType::kContinuous;
}

const std::vector<gfx::Rect>&
DevicePosturePlatformProviderDefault::GetViewportSegments() {
  return current_viewport_segments_;
}

void DevicePosturePlatformProviderDefault::StartListening() {}

void DevicePosturePlatformProviderDefault::StopListening() {}

}  // namespace device
