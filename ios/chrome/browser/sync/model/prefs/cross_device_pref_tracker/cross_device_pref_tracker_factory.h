// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_PREFS_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_PREFS_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_FACTORY_H_

#import <memory>

#import "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton factory that creates and manages one `CrossDevicePrefTracker`
// instance per `ProfileIOS`. The `CrossDevicePrefTracker` is responsible for
// observing and sharing select non-syncing preference values across a user's
// devices.
class CrossDevicePrefTrackerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          CrossDevicePrefTrackerFactory,
          sync_preferences::CrossDevicePrefTracker> {
 public:
  explicit CrossDevicePrefTrackerFactory(PassKey key);

 private:
  // `ProfileKeyedServiceFactoryIOS` implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_PREFS_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_FACTORY_H_
