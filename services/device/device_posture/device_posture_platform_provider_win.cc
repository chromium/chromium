// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_posture/device_posture_platform_provider_win.h"

namespace device {

DevicePosturePlatformProviderWin::DevicePosturePlatformProviderWin() = default;

DevicePosturePlatformProviderWin::~DevicePosturePlatformProviderWin() = default;

mojom::DevicePostureType DevicePosturePlatformProviderWin::GetDevicePosture() {
  return mojom::DevicePostureType::kContinuous;
}

void DevicePosturePlatformProviderWin::StartListening() {}

void DevicePosturePlatformProviderWin::StopListening() {}

}  // namespace device
