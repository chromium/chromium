// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_WIN_H_
#define CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_WIN_H_

#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "content/browser/device_posture/device_posture_platform_provider.h"

namespace content {

class DevicePostureRegistryWatcherWin;

class DevicePosturePlatformProviderWin : public DevicePosturePlatformProvider,
                                         public base::CheckedObserver {
 public:
  DevicePosturePlatformProviderWin();
  ~DevicePosturePlatformProviderWin() override;

  DevicePosturePlatformProviderWin(const DevicePosturePlatformProviderWin&) =
      delete;
  DevicePosturePlatformProviderWin& operator=(
      const DevicePosturePlatformProviderWin&) = delete;

  void UpdateDevicePosture(const blink::mojom::DevicePostureType& posture);
  void UpdateDisplayFeatureBounds(const gfx::Rect& display_feature_bounds);

 private:
  void StartListening() override;
  void StopListening() override;

  base::ScopedObservation<DevicePostureRegistryWatcherWin,
                          DevicePosturePlatformProviderWin>
      registry_watcher_observation_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_WIN_H_
