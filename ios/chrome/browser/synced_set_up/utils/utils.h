// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SET_UP_UTILS_UTILS_H_
#define IOS_CHROME_BROWSER_SYNCED_SET_UP_UTILS_UTILS_H_

#import <map>
#import <string_view>

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

@class ProfileState;
@class SceneState;

// Returns a map of tracked pref names and values corresponding to the "best
// match" prefs for the Synced Set Up flow to apply.
std::map<std::string_view, base::Value> GetRemoteDevicePrefs(
    const sync_preferences::CrossDevicePrefTracker* pref_tracker,
    const syncer::DeviceInfoTracker* device_info_tracker,
    const syncer::DeviceInfo* local_device);

// Helper that returns the name of a tracked pref for the current platform,
// given the corresponding cross device pref name.
std::string_view GetTrackedPrefName(
    const std::string_view& cross_device_pref_name);

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

#endif  // IOS_CHROME_BROWSER_SYNCED_SET_UP_UTILS_UTILS_H_
