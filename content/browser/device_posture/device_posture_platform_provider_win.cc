// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_posture/device_posture_platform_provider_win.h"

#include "content/browser/device_posture/device_posture_registry_watcher_win.h"

namespace content {

DevicePosturePlatformProviderWin::DevicePosturePlatformProviderWin() = default;

DevicePosturePlatformProviderWin::~DevicePosturePlatformProviderWin() = default;

void DevicePosturePlatformProviderWin::StartListening() {
  if (registry_watcher_observation_.IsObserving()) {
    return;
  }
  registry_watcher_observation_.Observe(
      DevicePostureRegistryWatcherWin::GetInstance());
}

void DevicePosturePlatformProviderWin::StopListening() {
  registry_watcher_observation_.Reset();
}

void DevicePosturePlatformProviderWin::UpdateDevicePosture(
    const blink::mojom::DevicePostureType& posture) {
  current_posture_ = posture;
  NotifyDevicePostureChanged(current_posture_);
}

void DevicePosturePlatformProviderWin::UpdateDisplayFeatureBounds(
    const gfx::Rect& display_feature_bounds) {
  current_display_feature_bounds_ = display_feature_bounds;
  NotifyDisplayFeatureBoundsChanged(current_display_feature_bounds_);
}

}  // namespace content
