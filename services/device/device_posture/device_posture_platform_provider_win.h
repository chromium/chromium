// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_WIN_H_
#define SERVICES_DEVICE_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_WIN_H_

#include <string_view>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "services/device/device_posture/device_posture_platform_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

class DevicePosturePlatformProviderWin : public DevicePosturePlatformProvider {
 public:
  DevicePosturePlatformProviderWin();
  ~DevicePosturePlatformProviderWin() override;

  DevicePosturePlatformProviderWin(const DevicePosturePlatformProviderWin&) =
      delete;
  DevicePosturePlatformProviderWin& operator=(
      const DevicePosturePlatformProviderWin&) = delete;

  device::mojom::DevicePostureType GetDevicePosture() override;
  const std::vector<gfx::Rect>& GetViewportSegments() override;
  void StartListening() override;
  void StopListening() override;

 private:
  friend class DevicePosturePlatformProviderWinTest;
  void OnRegistryKeyChanged();
  void ComputeFoldableState(const base::win::RegKey& registry_key,
                            bool notify_changes);
  static absl::optional<std::vector<gfx::Rect>> ParseViewportSegments(
      const base::Value::List& viewport_segments);
  static absl::optional<mojom::DevicePostureType> ParsePosture(
      std::string_view posture_state);

  mojom::DevicePostureType current_posture_ =
      mojom::DevicePostureType::kContinuous;
  std::vector<gfx::Rect> current_viewport_segments_;
  // This member is used to watch the registry after StartListening is called.
  // It will be destroyed when calling StopListening.
  absl::optional<base::win::RegKey> registry_key_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_WIN_H_
