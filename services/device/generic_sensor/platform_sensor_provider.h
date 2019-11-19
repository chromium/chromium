// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_H_

#include <memory>

#include "services/device/generic_sensor/platform_sensor_provider_base.h"

namespace device {

// This the base class for platform-specific sensor provider implementations.
// In typical usage a single instance is owned by DeviceService.
class PlatformSensorProvider : public PlatformSensorProviderBase {
 public:
  // Returns a PlatformSensorProvider for the current platform.
  // Note: returns 'nullptr' if there is no available implementation for
  // the current platform.
  static std::unique_ptr<PlatformSensorProvider> Create();

  ~PlatformSensorProvider() override = default;

 protected:
  PlatformSensorProvider() = default;

  // Determines if the ISensor or Windows.Devices.Sensors implementation
  // should be used on Windows.
  static bool UseWindowsWinrt();

  DISALLOW_COPY_AND_ASSIGN(PlatformSensorProvider);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_PROVIDER_H_
