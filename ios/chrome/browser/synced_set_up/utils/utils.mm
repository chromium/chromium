// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/utils/utils.h"

#import <Foundation/Foundation.h>

#import <optional>
#import <tuple>

#import "base/notreached.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/sync_device_info/device_info_tracker.h"
#import "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"
#import "components/sync_preferences/cross_device_pref_tracker/timestamped_pref_value.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

// Struct representing a unique device, containing a map of cross device pref
// names and values, and the total number of changes observed by the
// `CrossDevicePrefTracker` for the device.
struct DeviceData {
  // Map of all cross devices prefs and their values associated with this
  // device.
  std::map<std::string_view, sync_preferences::TimestampedPrefValue> pref_map;
  // Total number of observed pref changes on this device.
  size_t observed_change_count = 0;
};

// A map of device GUID's to device data containing the devices' respective sets
// of synced prefs and number of observed remote pref changes.
using DeviceDataMap = std::map<std::string, DeviceData>;

// Helper for adding `DeviceData` entries related to a `DeviceDataMap`, using
// prefs contained in `pref_map`.
template <size_t N>
void BuildDeviceDataMapFromPrefMap(
    DeviceDataMap& device_data_map,
    const sync_preferences::CrossDevicePrefTracker* pref_tracker,
    const base::fixed_flat_map<std::string_view, std::string_view, N>&
        pref_map) {
  sync_preferences::CrossDevicePrefTracker::DeviceFilter filter;

  // Query the tracker for tracked prefs and construct a `DeviceDataMap`
  // associating device GUID's with sets of prefs.
  for (const auto& tracked_pref : pref_map) {
    std::vector<sync_preferences::TimestampedPrefValue> pref_values =
        pref_tracker->GetValues(tracked_pref.first, filter);

    // Populate the device data map with device GUID's mapped to their set of
    // synced prefs and number of recent observed changes.
    for (const sync_preferences::TimestampedPrefValue& pref_value :
         pref_values) {
      DeviceData& device_data_entry =
          device_data_map[pref_value.device_sync_cache_guid];

      // Ensure that only the most recent observed pref change is preserved in
      // the DeviceData pref map.
      auto it = device_data_entry.pref_map.find(tracked_pref.first);
      if (it == device_data_entry.pref_map.end() ||
          pref_value.last_observed_change_time >=
              it->second.last_observed_change_time) {
        device_data_entry.pref_map.insert_or_assign(tracked_pref.first,
                                                    pref_value.Clone());
      }

      // Count total observed pref changes.
      if (!pref_value.last_observed_change_time.is_null()) {
        device_data_entry.observed_change_count++;
      }
    }
  }
}

// Returns a map of synced device GUID's to the devices' associated synced pref
// data.
DeviceDataMap MapPrefsToDevices(
    const sync_preferences::CrossDevicePrefTracker* pref_tracker) {
  DeviceDataMap device_data_map;
  BuildDeviceDataMapFromPrefMap(device_data_map, pref_tracker,
                                kCrossDeviceToProfilePrefMap);
  BuildDeviceDataMapFromPrefMap(device_data_map, pref_tracker,
                                kCrossDeviceToLocalStatePrefMap);
  return device_data_map;
}

// Returns the best fitting device in a `DeviceDataMap` of considered devices
// and their associated prefs.
DeviceData GetBestMatchDeviceData(
    DeviceDataMap& synced_devices,
    const syncer::DeviceInfoTracker* device_info_tracker,
    const syncer::DeviceInfo* local_device) {
  // Criteria for scoring devices against the local device as the best match for
  // applying synced prefs.
  using MatchesFormFactor = bool;
  using MatchesOsType = bool;
  using RecentPrefChangeCount = size_t;

  using DeviceScore =
      std::tuple<MatchesFormFactor, MatchesOsType, RecentPrefChangeCount>;

  // Devices cannot be scored.
  if (synced_devices.empty() || !device_info_tracker || !local_device) {
    return {};
  }

  // Use a std::set to automatically order the scores.
  // We store {score, guid} pairs. The set orders primarily by score.
  using ScoredDevice = std::pair<DeviceScore, std::string_view>;
  std::set<ScoredDevice> scored_remote_devices;

  const std::string& local_device_guid = local_device->guid();

  for (const auto& [guid, data] : synced_devices) {
    // 1. Skip local device and devices with no prefs.
    if (guid == local_device_guid || data.pref_map.empty()) {
      continue;
    }

    // 2. Ensure the device info exists.
    const syncer::DeviceInfo* device_info =
        device_info_tracker->GetDeviceInfo(guid);
    if (!device_info) {
      continue;
    }

    // 3. Calculate and insert the score.
    DeviceScore score = {
        device_info->form_factor() == local_device->form_factor(),
        device_info->os_type() == local_device->os_type(),
        data.observed_change_count};
    scored_remote_devices.insert({score, guid});
  }

  if (scored_remote_devices.empty()) {
    return {};
  }

  // The best device is the last element in the ordered set.
  const auto& best_match = *scored_remote_devices.rbegin();
  const std::string best_guid = std::string(best_match.second);

  // Final check: Compare activity level with the local device, if present.
  auto local_it = synced_devices.find(local_device_guid);
  if (local_it != synced_devices.end()) {
    if (local_it->second.observed_change_count >
        synced_devices.at(best_guid).observed_change_count) {
      // Local device has more activity; prefer to keep local settings.
      return {};
    }
  }
  return std::move(synced_devices.at(best_guid));
}

// Returns a map of tracked pref names and their values extracted from a set of
// `DeviceData`.
std::map<std::string_view, base::Value> GetCrossDevicePrefValuesForDevice(
    DeviceData& device) {
  std::map<std::string_view, base::Value> prefs;
  for (const auto& [cross_device_pref_name, timestamped_pref_value] :
       device.pref_map) {
    prefs.insert(std::make_pair(cross_device_pref_name,
                                timestamped_pref_value.value.Clone()));
  }
  return prefs;
}

}  // namespace

std::map<std::string_view, base::Value> GetCrossDevicePrefsFromRemoteDevice(
    const sync_preferences::CrossDevicePrefTracker* pref_tracker,
    const syncer::DeviceInfoTracker* device_info_tracker,
    const syncer::DeviceInfo* local_device) {
  if (!pref_tracker || !device_info_tracker || !local_device) {
    return {};
  }
  DeviceDataMap device_data_map = MapPrefsToDevices(pref_tracker);
  DeviceData best_match_device_data = GetBestMatchDeviceData(
      device_data_map, device_info_tracker, local_device);
  std::map<std::string_view, base::Value> cross_device_pref_values =
      GetCrossDevicePrefValuesForDevice(best_match_device_data);
  return cross_device_pref_values;
}

SceneState* GetEligibleSceneForSyncedSetUp(ProfileState* profile_state) {
  if (!profile_state) {
    return nil;
  }

  if (profile_state.initStage != ProfileInitStage::kFinal) {
    return nil;
  }

  if (profile_state.currentUIBlocker) {
    return nil;
  }

  SceneState* active_scene = profile_state.foregroundActiveScene;

  if (!active_scene) {
    return nil;
  }

  id<BrowserProviderInterface> provider_interface =
      active_scene.browserProviderInterface;
  id<BrowserProvider> presenting_interface =
      provider_interface.currentBrowserProvider;

  if (presenting_interface != provider_interface.mainBrowserProvider) {
    return nil;
  }

  // All preconditions met.
  return active_scene;
}

bool CanShowSyncedSetUp(const PrefService* profile_pref_service) {
  if (!profile_pref_service) {
    return false;
  }

  // Impressions preference not registered.
  if (!profile_pref_service->FindPreference(
          prefs::kSyncedSetUpImpressionCount)) {
    return false;
  }

  int impression_count =
      profile_pref_service->GetInteger(prefs::kSyncedSetUpImpressionCount);

  return impression_count < GetSyncedSetUpImpressionLimit();
}
