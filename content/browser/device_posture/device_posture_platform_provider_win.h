// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_WIN_H_
#define CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_WIN_H_

#include <string_view>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "content/browser/device_posture/device_posture_platform_provider.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class DevicePosturePlatformProviderWin : public DevicePosturePlatformProvider {
 public:
  DevicePosturePlatformProviderWin();
  ~DevicePosturePlatformProviderWin() override;

  DevicePosturePlatformProviderWin(const DevicePosturePlatformProviderWin&) =
      delete;
  DevicePosturePlatformProviderWin& operator=(
      const DevicePosturePlatformProviderWin&) = delete;

  blink::mojom::DevicePostureType GetDevicePosture() override;
  const std::vector<gfx::Rect>& GetViewportSegments() override;

 private:
  friend class DevicePosturePlatformProviderWinTest;

  void StartListening() override;
  void StopListening() override;
  void OnRegistryKeyChanged();
  void ComputeFoldableState(const base::win::RegKey& registry_key,
                            bool notify_changes);
  CONTENT_EXPORT static absl::optional<std::vector<gfx::Rect>>
  ParseViewportSegments(const base::Value::List& viewport_segments);
  CONTENT_EXPORT static absl::optional<blink::mojom::DevicePostureType>
  ParsePosture(std::string_view posture_state);

  blink::mojom::DevicePostureType current_posture_ =
      blink::mojom::DevicePostureType::kContinuous;
  std::vector<gfx::Rect> current_viewport_segments_;
  // This member is used to watch the registry after StartListening is called.
  // It will be destroyed when calling StopListening.
  absl::optional<base::win::RegKey> registry_key_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_WIN_H_
