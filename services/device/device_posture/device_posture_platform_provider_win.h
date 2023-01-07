// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_WIN_H_
#define SERVICES_DEVICE_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_WIN_H_

#include "services/device/device_posture/device_posture_platform_provider.h"

namespace device {

class DevicePosturePlatformProviderWin : public DevicePosturePlatformProvider {
 public:
  DevicePosturePlatformProviderWin();
  ~DevicePosturePlatformProviderWin() override;

  DevicePosturePlatformProviderWin(const DevicePosturePlatformProviderWin&) =
      delete;
  DevicePosturePlatformProviderWin& operator=(
      const DevicePosturePlatformProviderWin&) = delete;

  device::mojom::DevicePostureType GetDevicePosture() override;
  void StartListening() override;
  void StopListening() override;
};

}  // namespace device

#endif  // SERVICES_DEVICE_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_WIN_H_
