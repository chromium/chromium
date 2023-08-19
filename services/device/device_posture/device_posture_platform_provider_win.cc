// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/device_posture/device_posture_platform_provider_win.h"

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/json/json_reader.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
// The full specification of the registry is located over here
// https://github.com/foldable-devices/foldable-windows-registry-specification
// This approach is a stop gap solution until Windows gains proper APIs.
//
// TODO(darktears): When Windows gains the APIs we should update this code.
// https://crbug.com/1465934
//
// FOLED stands for Foldable OLED.
constexpr wchar_t kFoledRegKeyPath[] = L"Software\\Intel\\Foled";
}  // namespace

namespace device {

DevicePosturePlatformProviderWin::DevicePosturePlatformProviderWin() {
  base::win::RegKey registry_key(HKEY_CURRENT_USER, kFoledRegKeyPath,
                                 KEY_QUERY_VALUE);
  if (registry_key.Valid()) {
    ComputeFoldableState(registry_key, /*notify_changes=*/false);
  }
}

DevicePosturePlatformProviderWin::~DevicePosturePlatformProviderWin() = default;

mojom::DevicePostureType DevicePosturePlatformProviderWin::GetDevicePosture() {
  return current_posture_;
}

const std::vector<gfx::Rect>&
DevicePosturePlatformProviderWin::GetViewportSegments() {
  return current_viewport_segments_;
}

void DevicePosturePlatformProviderWin::StartListening() {
  if (registry_key_) {
    return;
  }

  registry_key_.emplace(HKEY_CURRENT_USER, kFoledRegKeyPath,
                        KEY_NOTIFY | KEY_QUERY_VALUE);
  if (registry_key_->Valid()) {
    // Start watching the registry for changes.
    registry_key_->StartWatching(
        base::BindOnce(&DevicePosturePlatformProviderWin::OnRegistryKeyChanged,
                       base::Unretained(this)));
  }
}

void DevicePosturePlatformProviderWin::StopListening() {
  registry_key_ = absl::nullopt;
}

absl::optional<mojom::DevicePostureType>
DevicePosturePlatformProviderWin::ParsePosture(
    base::StringPiece posture_state) {
  static constexpr auto kPostureStateToPostureType =
      base::MakeFixedFlatMap<base::StringPiece, mojom::DevicePostureType>(
          {{"MODE_HANDHELD", mojom::DevicePostureType::kFolded},
           {"MODE_DUAL_ANGLE", mojom::DevicePostureType::kFolded},
           {"MODE_LAPTOP_KB", mojom::DevicePostureType::kContinuous},
           {"MODE_LAYFLAT_LANDSCAPE", mojom::DevicePostureType::kContinuous},
           {"MODE_LAYFLAT_PORTRAIT", mojom::DevicePostureType::kContinuous},
           {"MODE_TABLETOP", mojom::DevicePostureType::kContinuous}});
  if (auto* iter = kPostureStateToPostureType.find(posture_state);
      iter != kPostureStateToPostureType.end()) {
    return iter->second;
  }
  DVLOG(1) << "Could not parse the posture data: " << posture_state;
  return absl::nullopt;
}

void DevicePosturePlatformProviderWin::ComputeFoldableState(
    const base::win::RegKey& registry_key,
    bool notify_changes) {
  CHECK(registry_key.Valid());

  std::wstring postureData;
  if (registry_key.ReadValue(L"PostureData", &postureData) != ERROR_SUCCESS) {
    return;
  }

  absl::optional<base::Value::Dict> dict =
      base::JSONReader::ReadDict(base::WideToUTF8(postureData));
  if (!dict) {
    DVLOG(1) << "Could not read the foldable status.";
    return;
  }
  const std::string* posture_state = dict->FindString("PostureState");
  if (!posture_state) {
    return;
  }

  const mojom::DevicePostureType old_posture = current_posture_;
  absl::optional<mojom::DevicePostureType> posture =
      ParsePosture(*posture_state);

  if (posture) {
    current_posture_ = posture.value();
    if (old_posture != current_posture_ && notify_changes) {
      NotifyDevicePostureChanged(current_posture_);
    }
  }

  base::Value::List* viewport_segments = dict->FindList("Rectangles");
  if (!viewport_segments) {
    DVLOG(1) << "Could not parse the viewport segments data.";
    return;
  }

  absl::optional<std::vector<gfx::Rect>> segments =
      ParseViewportSegments(*viewport_segments);
  if (!segments) {
    return;
  }

  current_viewport_segments_ = segments.value();
  if (notify_changes) {
    NotifyWindowSegmentsChanged(current_viewport_segments_);
  }
}

absl::optional<std::vector<gfx::Rect>>
DevicePosturePlatformProviderWin::ParseViewportSegments(
    const base::Value::List& viewport_segments) {
  if (viewport_segments.empty()) {
    return absl::nullopt;
  }

  // Check if the list is correctly constructed. It should be a multiple of
  // |left side|fold|right side| or 1.
  if (viewport_segments.size() != 1 && viewport_segments.size() % 3 != 0) {
    DVLOG(1) << "Could not parse the viewport segments data.";
    return absl::nullopt;
  }

  std::vector<gfx::Rect> segments;
  for (const auto& segment : viewport_segments) {
    const std::string* segment_string = segment.GetIfString();
    if (!segment_string) {
      DVLOG(1) << "Could not parse the viewport segments data";
      return absl::nullopt;
    }
    auto rectangle_dimensions = base::SplitStringPiece(
        *segment_string, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
        base::SplitResult::SPLIT_WANT_NONEMPTY);
    if (rectangle_dimensions.size() != 4) {
      DVLOG(1) << "Could not parse the viewport segments data: "
               << *segment_string;
      return absl::nullopt;
    }
    int x, y, width, height;
    if (!base::StringToInt(rectangle_dimensions[0], &x) ||
        !base::StringToInt(rectangle_dimensions[1], &y) ||
        !base::StringToInt(rectangle_dimensions[2], &width) ||
        !base::StringToInt(rectangle_dimensions[3], &height)) {
      DVLOG(1) << "Could not parse the viewport segments data: "
               << *segment_string;
      return absl::nullopt;
    }
    segments.emplace_back(x, y, width, height);
  }
  return segments;
}

void DevicePosturePlatformProviderWin::OnRegistryKeyChanged() {
  // |OnRegistryKeyChanged| is removed as an observer when the ChangeCallback is
  // called, so we need to re-register.
  registry_key_->StartWatching(
      base::BindOnce(&DevicePosturePlatformProviderWin::OnRegistryKeyChanged,
                     base::Unretained(this)));
  ComputeFoldableState(registry_key_.value(), /*notify_changes=*/true);
}

}  // namespace device
