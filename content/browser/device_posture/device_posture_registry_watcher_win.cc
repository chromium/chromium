// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/device_posture/device_posture_registry_watcher_win.h"

#include <optional>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/json/json_reader.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"

using blink::mojom::DevicePostureType;

namespace {
// The full specification of the registry is located over here
// https://github.com/foldable-devices/foldable-windows-registry-specification
// This approach is a stop gap solution until Windows gains proper APIs.
//
// TODO(crbug.com/40276180): When Windows gains the APIs we should update this
// code.
//
// FOLED stands for Foldable OLED.
constexpr wchar_t kFoledRegKeyPath[] = L"Software\\Intel\\Foled";

// On Windows the platform returns [left][fold][right] and so far we support
// only one display feature in Chromium.
constexpr int kFirstFoldInSegmentsArray = 1;

}  // namespace

namespace content {
// static
DevicePostureRegistryWatcherWin*
DevicePostureRegistryWatcherWin::GetInstance() {
  static base::NoDestructor<DevicePostureRegistryWatcherWin> instance;
  return instance.get();
}

DevicePostureRegistryWatcherWin::DevicePostureRegistryWatcherWin() {
  base::win::RegKey registry_key(HKEY_CURRENT_USER, kFoledRegKeyPath,
                                 KEY_QUERY_VALUE);
  if (registry_key.Valid()) {
    ComputeFoldableState(registry_key, /*notify_changes=*/false);
  }
}

DevicePostureRegistryWatcherWin::~DevicePostureRegistryWatcherWin() = default;

void DevicePostureRegistryWatcherWin::AddObserver(
    DevicePosturePlatformProviderWin* observer) {
  if (!observer) {
    return;
  }

  DCHECK(!observers_.HasObserver(observer));
  if (observers_.empty()) {
    DCHECK(!registry_key_);
    registry_key_.emplace(HKEY_CURRENT_USER, kFoledRegKeyPath,
                          KEY_NOTIFY | KEY_QUERY_VALUE);
    if (registry_key_->Valid()) {
      // Start watching the registry for changes.
      registry_key_->StartWatching(
          base::BindOnce(&DevicePostureRegistryWatcherWin::OnRegistryKeyChanged,
                         base::Unretained(this)));
    }
  }

  observers_.AddObserver(observer);
  // Inform the observer with the current state.
  observer->UpdateDevicePosture(current_posture_);
  observer->UpdateDisplayFeatureBounds(current_display_feature_bounds_);
}

void DevicePostureRegistryWatcherWin::RemoveObserver(
    DevicePosturePlatformProviderWin* observer) {
  observers_.RemoveObserver(observer);
  if (observers_.empty()) {
    registry_key_ = std::nullopt;
  }
}

std::optional<DevicePostureType> DevicePostureRegistryWatcherWin::ParsePosture(
    std::string_view posture_state) {
  static constexpr auto kPostureStateToPostureType =
      base::MakeFixedFlatMap<std::string_view, DevicePostureType>(
          {{"MODE_HANDHELD", DevicePostureType::kFolded},
           {"MODE_DUAL_ANGLE", DevicePostureType::kFolded},
           {"MODE_LAPTOP_KB", DevicePostureType::kContinuous},
           {"MODE_LAYFLAT_LANDSCAPE", DevicePostureType::kContinuous},
           {"MODE_LAYFLAT_PORTRAIT", DevicePostureType::kContinuous},
           {"MODE_TABLETOP", DevicePostureType::kContinuous}});
  if (auto iter = kPostureStateToPostureType.find(posture_state);
      iter != kPostureStateToPostureType.end()) {
    return iter->second;
  }
  DVLOG(1) << "Could not parse the posture data: " << posture_state;
  return std::nullopt;
}

void DevicePostureRegistryWatcherWin::ComputeFoldableState(
    const base::win::RegKey& registry_key,
    bool notify_changes) {
  CHECK(registry_key.Valid());

  std::wstring posture_data;
  if (registry_key.ReadValue(L"PostureData", &posture_data) != ERROR_SUCCESS) {
    return;
  }

  std::optional<base::Value::Dict> dict =
      base::JSONReader::ReadDict(base::WideToUTF8(posture_data));
  if (!dict) {
    DVLOG(1) << "Could not read the foldable status.";
    return;
  }
  const std::string* posture_state = dict->FindString("PostureState");
  if (!posture_state) {
    return;
  }

  const DevicePostureType old_posture = current_posture_;
  std::optional<DevicePostureType> posture = ParsePosture(*posture_state);

  if (posture) {
    current_posture_ = posture.value();
    if (old_posture != current_posture_ && notify_changes) {
      for (DevicePosturePlatformProviderWin& observer : observers_) {
        observer.UpdateDevicePosture(current_posture_);
      }
    }
  }

  base::Value::List* viewport_segments = dict->FindList("Rectangles");
  if (!viewport_segments) {
    DVLOG(1) << "Could not parse the viewport segments data.";
    return;
  }

  std::optional<std::vector<gfx::Rect>> segments =
      ParseViewportSegments(*viewport_segments);
  if (!segments) {
    return;
  }

  // If there is not enough segments then the display feature is empty.
  if (segments->size() < 2) {
    current_display_feature_bounds_ = gfx::Rect();
  } else {
    // We want the first fold segment of the segment array.
    current_display_feature_bounds_ = segments->at(kFirstFoldInSegmentsArray);
  }

  if (notify_changes) {
    for (DevicePosturePlatformProviderWin& observer : observers_) {
      observer.UpdateDisplayFeatureBounds(current_display_feature_bounds_);
    }
  }
}

std::optional<std::vector<gfx::Rect>>
DevicePostureRegistryWatcherWin::ParseViewportSegments(
    const base::Value::List& viewport_segments) {
  if (viewport_segments.empty()) {
    return std::nullopt;
  }

  // Check if the list is correctly constructed. It should be a multiple of
  // |left side|fold|right side| or 1.
  if (viewport_segments.size() != 1 && viewport_segments.size() % 3 != 0) {
    DVLOG(1) << "Could not parse the viewport segments data.";
    return std::nullopt;
  }

  std::vector<gfx::Rect> segments;
  for (const auto& segment : viewport_segments) {
    const std::string* segment_string = segment.GetIfString();
    if (!segment_string) {
      DVLOG(1) << "Could not parse the viewport segments data";
      return std::nullopt;
    }
    auto rectangle_dimensions = base::SplitStringPiece(
        *segment_string, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
        base::SplitResult::SPLIT_WANT_NONEMPTY);
    if (rectangle_dimensions.size() != 4) {
      DVLOG(1) << "Could not parse the viewport segments data: "
               << *segment_string;
      return std::nullopt;
    }
    int x, y, width, height;
    if (!base::StringToInt(rectangle_dimensions[0], &x) ||
        !base::StringToInt(rectangle_dimensions[1], &y) ||
        !base::StringToInt(rectangle_dimensions[2], &width) ||
        !base::StringToInt(rectangle_dimensions[3], &height)) {
      DVLOG(1) << "Could not parse the viewport segments data: "
               << *segment_string;
      return std::nullopt;
    }
    segments.emplace_back(x, y, width, height);
  }
  return segments;
}

void DevicePostureRegistryWatcherWin::OnRegistryKeyChanged() {
  // |OnRegistryKeyChanged| is removed as an observer when the ChangeCallback is
  // called, so we need to re-register.
  registry_key_->StartWatching(
      base::BindOnce(&DevicePostureRegistryWatcherWin::OnRegistryKeyChanged,
                     base::Unretained(this)));
  ComputeFoldableState(registry_key_.value(), /*notify_changes=*/true);
}

}  // namespace content
