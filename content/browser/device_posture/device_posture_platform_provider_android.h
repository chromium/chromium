// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_ANDROID_H_
#define CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_ANDROID_H_

#include "content/browser/device_posture/device_posture_platform_provider.h"

namespace content {

class DevicePosturePlatformProviderAndroid
    : public DevicePosturePlatformProvider {
 public:
  DevicePosturePlatformProviderAndroid();
  ~DevicePosturePlatformProviderAndroid() override;

  DevicePosturePlatformProviderAndroid(
      const DevicePosturePlatformProviderAndroid&) = delete;
  DevicePosturePlatformProviderAndroid& operator=(
      const DevicePosturePlatformProviderAndroid&) = delete;

  blink::mojom::DevicePostureType GetDevicePosture() override;
  const std::vector<gfx::Rect>& GetViewportSegments() override;
  void StartListening() override;
  void StopListening() override;

 private:
  std::vector<gfx::Rect> current_viewport_segments_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_ANDROID_H_
