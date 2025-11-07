// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SET_UP_UTILS_UTILS_H_
#define IOS_CHROME_BROWSER_SYNCED_SET_UP_UTILS_UTILS_H_

#import <map>
#import <string_view>

#import "base/containers/fixed_flat_map.h"
#import "components/commerce/core/pref_names.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/safety_check/safety_check_pref_names.h"
#import "components/sync_preferences/cross_device_pref_tracker/prefs/cross_device_pref_names.h"

namespace base {
class Value;
}  // namespace base

namespace syncer {
class DeviceInfo;
class DeviceInfoTracker;
}  // namespace syncer

namespace sync_preferences {
class CrossDevicePrefTracker;
}  // namespace sync_preferences

class PrefService;
@class ProfileState;
@class SceneState;

// Map of cross-device synced prefs considered by Synced Set Up mapped to their
// corresponding tracked local-state pref.
inline constexpr auto kCrossDeviceToLocalStatePrefMap =
    base::MakeFixedFlatMap<std::string_view, std::string_view>({
        // keep-sorted start
        {prefs::kCrossDeviceOmniboxIsInBottomPosition,
         omnibox::kIsOmniboxInBottomPosition},
        // keep-sorted end
    });

// Map of cross device synced prefs considered by Synced Set Up mapped to their
// corresponding tracked profile pref.
inline constexpr auto kCrossDeviceToProfilePrefMap =
    base::MakeFixedFlatMap<std::string_view, std::string_view>({
        // keep-sorted start
        {prefs::kCrossDeviceMagicStackHomeModuleEnabled,
         ntp_tiles::prefs::kMagicStackHomeModuleEnabled},
        {prefs::kCrossDeviceMostVisitedHomeModuleEnabled,
         ntp_tiles::prefs::kMostVisitedHomeModuleEnabled},
        {prefs::kCrossDevicePriceTrackingHomeModuleEnabled,
         commerce::kPriceTrackingHomeModuleEnabled},
        {prefs::kCrossDeviceSafetyCheckHomeModuleEnabled,
         safety_check::prefs::kSafetyCheckHomeModuleEnabled},
        {prefs::kCrossDeviceTabResumptionHomeModuleEnabled,
         ntp_tiles::prefs::kTabResumptionHomeModuleEnabled},
        {prefs::kCrossDeviceTipsHomeModuleEnabled,
         ntp_tiles::prefs::kTipsHomeModuleEnabled},
        // keep-sorted end
    });

// Returns a map of tracked pref names and values corresponding to the "best
// match" prefs for the Synced Set Up flow to apply.
std::map<std::string_view, base::Value> GetCrossDevicePrefsFromRemoteDevice(
    const sync_preferences::CrossDevicePrefTracker* pref_tracker,
    const syncer::DeviceInfoTracker* device_info_tracker,
    const syncer::DeviceInfo* local_device);

// Returns the active, non-incognito `SceneState` if preconditions for
// triggering the Synced Set Up flow are met based on `profile_state`, and `nil`
// otherwise.
//
// Preconditions include:
// - Profile initialization is complete.
// - There is no current UI blocker.
// - There is a foreground active scene.
// - The active scene is not incognito and belongs to the main browser provider.
SceneState* GetEligibleSceneForSyncedSetUp(ProfileState* profile_state);

// Returns true if the Synced Set Up UI can be shown based on the impression
// limit. This should be passed a profile pref service.
bool CanShowSyncedSetUp(const PrefService* profile_pref_service);

#endif  // IOS_CHROME_BROWSER_SYNCED_SET_UP_UTILS_UTILS_H_
