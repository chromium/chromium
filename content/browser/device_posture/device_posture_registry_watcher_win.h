// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_REGISTRY_WATCHER_WIN_H_
#define CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_REGISTRY_WATCHER_WIN_H_

#include <optional>
#include <string_view>
#include <vector>

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "content/browser/device_posture/device_posture_platform_provider_win.h"
#include "content/common/content_export.h"

namespace content {

// This is a singleton class that will fetch and monitor the registry to get
// posture and display feature information.
class DevicePostureRegistryWatcherWin {
 public:
  static DevicePostureRegistryWatcherWin* GetInstance();

  void AddObserver(DevicePosturePlatformProviderWin* observer);
  void RemoveObserver(DevicePosturePlatformProviderWin* observer);

  DevicePostureRegistryWatcherWin(const DevicePostureRegistryWatcherWin&) =
      delete;
  DevicePostureRegistryWatcherWin& operator=(
      const DevicePostureRegistryWatcherWin&) = delete;

 private:
  friend class DevicePostureRegistryWatcherWinTest;
  friend class base::NoDestructor<DevicePostureRegistryWatcherWin>;

  DevicePostureRegistryWatcherWin();
  ~DevicePostureRegistryWatcherWin();

  void OnRegistryKeyChanged();
  void ComputeFoldableState(const base::win::RegKey& registry_key,
                            bool notify_changes);
  CONTENT_EXPORT static std::optional<std::vector<gfx::Rect>>
  ParseViewportSegments(const base::Value::List& viewport_segments);
  CONTENT_EXPORT static std::optional<blink::mojom::DevicePostureType>
  ParsePosture(std::string_view posture_state);

  base::ObserverList<DevicePosturePlatformProviderWin> observers_;

  blink::mojom::DevicePostureType current_posture_ =
      blink::mojom::DevicePostureType::kContinuous;
  gfx::Rect current_display_feature_bounds_;

  // This member is used to watch the registry after StartListening is called.
  // It will be destroyed when calling StopListening.
  std::optional<base::win::RegKey> registry_key_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_REGISTRY_WATCHER_WIN_H_
