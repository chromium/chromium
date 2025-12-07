// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_PREFS_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_PREFS_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import <string_view>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"

namespace syncer {
class DeviceInfo;
}  // namespace syncer
namespace sync_preferences {
struct TimestampedPrefValue;
}  // namespace sync_preferences

// Objective-C protocol mirroring `CrossDevicePrefTracker::Observer`.
@protocol CrossDevicePrefTrackerObserver

// Called when `prefName` is updated to `prefValue` on a remote device.
// The `prefName` reported here is always the tracked pref name on iOS (e.g.,
// "ios.example_pref").
- (void)onRemotePrefChanged:(std::string_view)prefName
                  prefValue:
                      (const sync_preferences::TimestampedPrefValue&)prefValue
           remoteDeviceInfo:(const syncer::DeviceInfo&)remoteDeviceInfo;

@end

// Simple observer bridge that forwards `CrossDevicePrefTracker` events to its
// delegate observer.
class CrossDevicePrefTrackerObserverBridge
    : public sync_preferences::CrossDevicePrefTracker::Observer {
 public:
  CrossDevicePrefTrackerObserverBridge(
      id<CrossDevicePrefTrackerObserver> delegate,
      sync_preferences::CrossDevicePrefTracker* tracker);

  ~CrossDevicePrefTrackerObserverBridge() override;

  // `sync_preferences::CrossDevicePrefTracker::Observer` implementation.
  void OnRemotePrefChanged(
      std::string_view pref_name,
      const sync_preferences::TimestampedPrefValue& pref_value,
      const syncer::DeviceInfo& remote_device_info) override;

 private:
  __weak id<CrossDevicePrefTrackerObserver> delegate_ = nil;

  // Scoped observation handles the automatic registration and removal of the
  // observer from the tracker.
  base::ScopedObservation<sync_preferences::CrossDevicePrefTracker,
                          sync_preferences::CrossDevicePrefTracker::Observer>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_PREFS_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_OBSERVER_BRIDGE_H_
