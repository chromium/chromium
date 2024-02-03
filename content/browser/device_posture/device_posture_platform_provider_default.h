// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_DEFAULT_H_
#define CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_DEFAULT_H_

#include "content/browser/device_posture/device_posture_platform_provider.h"

namespace content {

class DevicePosturePlatformProviderDefault
    : public DevicePosturePlatformProvider {
 public:
  DevicePosturePlatformProviderDefault();
  ~DevicePosturePlatformProviderDefault() override;

  DevicePosturePlatformProviderDefault(
      const DevicePosturePlatformProviderDefault&) = delete;
  DevicePosturePlatformProviderDefault& operator=(
      const DevicePosturePlatformProviderDefault&) = delete;

  void StartListening() override;
  void StopListening() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_DEFAULT_H_
