// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_WIN_H_
#define CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_WIN_H_

#include <optional>
#include <string_view>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "content/browser/device_posture/device_posture_platform_provider.h"
#include "content/common/content_export.h"

namespace content {

class DevicePosturePlatformProviderWin : public DevicePosturePlatformProvider {
 public:
  DevicePosturePlatformProviderWin();
  ~DevicePosturePlatformProviderWin() override;

  DevicePosturePlatformProviderWin(const DevicePosturePlatformProviderWin&) =
      delete;
  DevicePosturePlatformProviderWin& operator=(
      const DevicePosturePlatformProviderWin&) = delete;

 private:
  friend class DevicePosturePlatformProviderWinTest;

  void StartListening() override;
  void StopListening() override;
  void OnRegistryKeyChanged();
  void ComputeFoldableState(const base::win::RegKey& registry_key,
                            bool notify_changes);
  CONTENT_EXPORT static std::optional<std::vector<gfx::Rect>>
  ParseViewportSegments(const base::Value::List& viewport_segments);
  CONTENT_EXPORT static std::optional<blink::mojom::DevicePostureType>
  ParsePosture(std::string_view posture_state);

  // This member is used to watch the registry after StartListening is called.
  // It will be destroyed when calling StopListening.
  std::optional<base::win::RegKey> registry_key_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_WIN_H_
