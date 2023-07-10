// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_posture/device_posture_platform_provider_win.h"

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
// The full specification of the registry is located over here
// https://github.com/foldable-devices/foldable-windows-registry-specification
// This approach is a stop gap solution until Windows gains proper APIs
constexpr auto kFoldedPostures = base::MakeFixedFlatSet<base::StringPiece>(
    {"MODE_HANDHELD", "MODE_DUAL_ANGLE"});
constexpr auto kContinuousPostures = base::MakeFixedFlatSet<base::StringPiece>(
    {"MODE_LAYFLAT_LANDSCAPE", "MODE_LAYFLAT_PORTRAIT", "MODE_TABLETOP",
     "MODE_LAPTOP_KB"});
constexpr wchar_t kFoledRegKeyPath[] = L"Software\\Intel\\Foled";
}  // namespace

namespace device {

DevicePosturePlatformProviderWin::DevicePosturePlatformProviderWin()
    : registry_key_(HKEY_CURRENT_USER, kFoledRegKeyPath, KEY_NOTIFY | KEY_READ),
      main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  if (registry_key_.Valid()) {
    ComputePosture();
    initialized_ = true;
  }
}

DevicePosturePlatformProviderWin::~DevicePosturePlatformProviderWin() = default;

mojom::DevicePostureType DevicePosturePlatformProviderWin::GetDevicePosture() {
  return current_posture_;
}

void DevicePosturePlatformProviderWin::StartListening() {
  registry_key_ = base::win::RegKey(HKEY_CURRENT_USER, kFoledRegKeyPath,
                                    KEY_NOTIFY | KEY_READ);
  if (registry_key_.Valid()) {
    // Start watching the registry for changes.
    registry_key_.StartWatching(
        base::BindOnce(&DevicePosturePlatformProviderWin::OnRegistryKeyChanged,
                       base::Unretained(this)));
  }
}

void DevicePosturePlatformProviderWin::StopListening() {
  registry_key_.Close();
}

void DevicePosturePlatformProviderWin::ComputePosture() {
  CHECK(registry_key_.Valid());

  std::wstring postureData;
  if (registry_key_.ReadValue(L"PostureData", &postureData) != ERROR_SUCCESS) {
    return;
  }

  absl::optional<base::Value::Dict> dict =
      base::JSONReader::ReadDict(base::WideToUTF8(postureData));
  if (!dict) {
    LOG(ERROR) << "Could not read the foldable status.";
    return;
  }
  const std::string* posture_state = dict->FindString("PostureState");
  if (!posture_state) {
    return;
  }

  const mojom::DevicePostureType old_posture = current_posture_;
  if (base::Contains(kFoldedPostures, *posture_state)) {
    current_posture_ = mojom::DevicePostureType::kFolded;
  } else if (base::Contains(kContinuousPostures, *posture_state)) {
    current_posture_ = mojom::DevicePostureType::kContinuous;
  } else {
    LOG(ERROR) << "Could not parse the posture data.";
    return;
  }

  if (old_posture != current_posture_ && initialized_) {
    NotifyDevicePostureChanged(current_posture_);
  }
}

void DevicePosturePlatformProviderWin::OnRegistryKeyChanged() {
  // |OnRegistryKeyChanged| is removed as an observer when the ChangeCallback is
  // called, so we need to re-register.
  CHECK(registry_key_.Valid());

  registry_key_.StartWatching(
      base::BindOnce(&DevicePosturePlatformProviderWin::OnRegistryKeyChanged,
                     base::Unretained(this)));
  ComputePosture();
}

}  // namespace device
