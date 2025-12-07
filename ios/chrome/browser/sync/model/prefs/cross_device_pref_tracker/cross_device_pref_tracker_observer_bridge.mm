// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/prefs/cross_device_pref_tracker/cross_device_pref_tracker_observer_bridge.h"

#import "base/check.h"
#import "components/sync_device_info/device_info.h"
#import "components/sync_preferences/cross_device_pref_tracker/timestamped_pref_value.h"

CrossDevicePrefTrackerObserverBridge::CrossDevicePrefTrackerObserverBridge(
    id<CrossDevicePrefTrackerObserver> delegate,
    sync_preferences::CrossDevicePrefTracker* tracker)
    : delegate_(delegate) {
  CHECK(delegate_);
  CHECK(tracker);

  scoped_observation_.Observe(tracker);
}

CrossDevicePrefTrackerObserverBridge::~CrossDevicePrefTrackerObserverBridge() =
    default;

void CrossDevicePrefTrackerObserverBridge::OnRemotePrefChanged(
    std::string_view pref_name,
    const sync_preferences::TimestampedPrefValue& pref_value,
    const syncer::DeviceInfo& remote_device_info) {
  [delegate_ onRemotePrefChanged:pref_name
                       prefValue:pref_value
                remoteDeviceInfo:remote_device_info];
}
