// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_H_
#define SERVICES_DEVICE_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "services/device/public/mojom/device_posture_provider.mojom.h"

namespace device {

class DevicePostureProviderImpl;

// This the base class for platform-specific device posture provider
// implementations. In typical usage a single instance is owned by
// DeviceService.
class DevicePosturePlatformProvider {
 public:
  // Returns a DevicePostureProvider for the current platform.
  // Note: returns 'nullptr' if there is no available implementation for
  // the current platform.
  static std::unique_ptr<DevicePosturePlatformProvider> Create();

  virtual ~DevicePosturePlatformProvider() = default;

  virtual device::mojom::DevicePostureType GetDevicePosture() = 0;
  virtual const std::vector<gfx::Rect>& GetViewportSegments() = 0;
  virtual void StopListening() = 0;
  virtual void StartListening() = 0;

  void SetPostureProvider(DevicePostureProviderImpl* provider);

  DevicePosturePlatformProvider(const DevicePosturePlatformProvider&) = delete;
  DevicePosturePlatformProvider& operator=(
      const DevicePosturePlatformProvider&) = delete;

 protected:
  DevicePosturePlatformProvider() = default;
  void NotifyDevicePostureChanged(const mojom::DevicePostureType& posture);
  void NotifyWindowSegmentsChanged(const std::vector<gfx::Rect>& segments);

 private:
  // DevicePosturePlatformProvider is created and owned by
  // DevicePostureProviderImpl making it safe to hold a raw pointer.
  raw_ptr<DevicePostureProviderImpl> provider_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_H_
