// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_posture/device_posture_platform_provider.h"

#include "build/build_config.h"
#include "content/browser/device_posture/device_posture_platform_provider_default.h"

#if BUILDFLAG(IS_WIN)
#include "content/browser/device_posture/device_posture_platform_provider_win.h"
#elif BUILDFLAG(IS_ANDROID)
#include "content/browser/device_posture/device_posture_platform_provider_android.h"
#endif

namespace content {

// static
std::unique_ptr<DevicePosturePlatformProvider>
DevicePosturePlatformProvider::Create(WebContents* web_contents) {
#if BUILDFLAG(IS_WIN)
  return std::make_unique<DevicePosturePlatformProviderWin>();
#elif BUILDFLAG(IS_ANDROID)
  return std::make_unique<DevicePosturePlatformProviderAndroid>(web_contents);
#else
  return std::make_unique<DevicePosturePlatformProviderDefault>();
#endif
}

DevicePosturePlatformProvider::DevicePosturePlatformProvider() = default;

DevicePosturePlatformProvider::~DevicePosturePlatformProvider() = default;

blink::mojom::DevicePostureType
DevicePosturePlatformProvider::GetDevicePosture() {
  return current_posture_;
}

const gfx::Rect& DevicePosturePlatformProvider::GetDisplayFeatureBounds() {
  return current_display_feature_bounds_;
}

void DevicePosturePlatformProvider::AddObserver(Observer* observer) {
  if (observers_.empty()) {
    StartListening();
  }
  observers_.AddObserver(observer);
}
void DevicePosturePlatformProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
  if (observers_.empty()) {
    StopListening();
  }
}

void DevicePosturePlatformProvider::NotifyDevicePostureChanged(
    const blink::mojom::DevicePostureType& posture) {
  for (auto& observer : observers_) {
    observer.OnDevicePostureChanged(posture);
  }
}

void DevicePosturePlatformProvider::NotifyDisplayFeatureBoundsChanged(
    const gfx::Rect& display_feature_bounds) {
  for (auto& observer : observers_) {
    observer.OnDisplayFeatureBoundsChanged(display_feature_bounds);
  }
}

}  // namespace content
