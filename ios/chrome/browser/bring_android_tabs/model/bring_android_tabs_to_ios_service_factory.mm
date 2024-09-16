// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/model/bring_android_tabs_to_ios_service_factory.h"

#import "base/feature_list.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/features.h"
#import "ios/chrome/browser/bring_android_tabs/model/bring_android_tabs_to_ios_service.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

// static
BringAndroidTabsToIOSService*
BringAndroidTabsToIOSServiceFactory::GetForBrowserState(ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
BringAndroidTabsToIOSService*
BringAndroidTabsToIOSServiceFactory::GetForProfile(ProfileIOS* profile) {
  DCHECK(!profile->IsOffTheRecord());
  return static_cast<BringAndroidTabsToIOSService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
BringAndroidTabsToIOSService*
BringAndroidTabsToIOSServiceFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return static_cast<BringAndroidTabsToIOSService*>(
      GetInstance()->GetServiceForBrowserState(profile, false));
}

// static
BringAndroidTabsToIOSServiceFactory*
BringAndroidTabsToIOSServiceFactory::GetInstance() {
  static base::NoDestructor<BringAndroidTabsToIOSServiceFactory> instance;
  return instance.get();
}

BringAndroidTabsToIOSServiceFactory::BringAndroidTabsToIOSServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "BringAndroidTabsToIOSService",
          BrowserStateDependencyManager::GetInstance()) {}

BringAndroidTabsToIOSServiceFactory::~BringAndroidTabsToIOSServiceFactory() {
  DependsOn(
      segmentation_platform::SegmentationPlatformServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
}

void BringAndroidTabsToIOSServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kIosBringAndroidTabsPromptDisplayed,
                                false);
}

std::unique_ptr<KeyedService>
BringAndroidTabsToIOSServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  // SegmentationPlatform is required for BringYourOwnTabsIOS to work.
  if (!base::FeatureList::IsEnabled(
          segmentation_platform::features::kSegmentationPlatformFeature)) {
    return nullptr;
  }

  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  PrefService* profile_prefs = profile ? profile->GetPrefs() : nullptr;
  return std::make_unique<BringAndroidTabsToIOSService>(
      segmentation_platform::SegmentationPlatformServiceFactory::
          GetDispatcherForProfile(profile),
      SyncServiceFactory::GetForProfile(profile),
      SessionSyncServiceFactory::GetForProfile(profile), profile_prefs);
}
