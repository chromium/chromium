// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_PREFS_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_PREFS_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class KeyedService;
class ProfileIOS;

namespace sync_preferences {
class CrossDevicePrefTracker;
}  // namespace sync_preferences

// Singleton factory that creates and manages one `CrossDevicePrefTracker`
// instance per `ProfileIOS`. The `CrossDevicePrefTracker` is responsible for
// observing and sharing select non-syncing preference values across a user's
// devices.
class CrossDevicePrefTrackerFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static CrossDevicePrefTrackerFactory* GetInstance();

  // Returns the `CrossDevicePrefTracker` associated with `profile`.
  // If no instance exists, one will be created.
  static sync_preferences::CrossDevicePrefTracker* GetForProfile(
      ProfileIOS* profile);

  CrossDevicePrefTrackerFactory(const CrossDevicePrefTrackerFactory&) = delete;
  CrossDevicePrefTrackerFactory& operator=(
      const CrossDevicePrefTrackerFactory&) = delete;

 private:
  friend class base::NoDestructor<CrossDevicePrefTrackerFactory>;

  CrossDevicePrefTrackerFactory();
  ~CrossDevicePrefTrackerFactory() override;

  // `ProfileKeyedServiceFactoryIOS` implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_PREFS_CROSS_DEVICE_PREF_TRACKER_CROSS_DEVICE_PREF_TRACKER_FACTORY_H_
