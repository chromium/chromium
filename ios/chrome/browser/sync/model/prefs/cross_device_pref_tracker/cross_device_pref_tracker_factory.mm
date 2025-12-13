// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/prefs/cross_device_pref_tracker/cross_device_pref_tracker_factory.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"
#import "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker_impl.h"
#import "components/sync_preferences/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/prefs/cross_device_pref_tracker/ios_chrome_cross_device_pref_provider.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

// static
sync_preferences::CrossDevicePrefTracker*
CrossDevicePrefTrackerFactory::GetForProfile(ProfileIOS* profile) {
  // The factory configuration ensures this is only called for the original
  // profile.
  CHECK(!profile->IsOffTheRecord());

  return GetInstance()
      ->GetServiceForProfileAs<sync_preferences::CrossDevicePrefTracker>(
          profile, /*create=*/true);
}

// static
CrossDevicePrefTrackerFactory* CrossDevicePrefTrackerFactory::GetInstance() {
  static base::NoDestructor<CrossDevicePrefTrackerFactory> instance;
  return instance.get();
}

CrossDevicePrefTrackerFactory::CrossDevicePrefTrackerFactory()
    : ProfileKeyedServiceFactoryIOS("CrossDevicePrefTracker",
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

CrossDevicePrefTrackerFactory::~CrossDevicePrefTrackerFactory() = default;

std::unique_ptr<KeyedService>
CrossDevicePrefTrackerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(
          sync_preferences::features::kEnableCrossDevicePrefTracker)) {
    return nullptr;
  }

  auto pref_provider = std::make_unique<IOSChromeCrossDevicePrefProvider>();

  // The implementation in `components/sync_preferences` is platform-agnostic.
  return std::make_unique<sync_preferences::CrossDevicePrefTrackerImpl>(
      profile->GetPrefs(), GetApplicationContext()->GetLocalState(),
      DeviceInfoSyncServiceFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile), std::move(pref_provider));
}
