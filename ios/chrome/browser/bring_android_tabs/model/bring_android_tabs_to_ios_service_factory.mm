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
#import "ios/chrome/browser/bring_android_tabs/model/features.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

// static
BringAndroidTabsToIOSService*
BringAndroidTabsToIOSServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  DCHECK(!browser_state->IsOffTheRecord());
  return static_cast<BringAndroidTabsToIOSService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
BringAndroidTabsToIOSService*
BringAndroidTabsToIOSServiceFactory::GetForBrowserStateIfExists(
    ChromeBrowserState* browser_state) {
  return static_cast<BringAndroidTabsToIOSService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
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
  if (!base::FeatureList::IsEnabled(kBringYourOwnTabsIOS) ||
      !base::FeatureList::IsEnabled(
          segmentation_platform::features::kSegmentationPlatformFeature)) {
    return nullptr;
  }

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  PrefService* browser_state_prefs =
      browser_state ? browser_state->GetPrefs() : nullptr;
  return std::make_unique<BringAndroidTabsToIOSService>(
      segmentation_platform::SegmentationPlatformServiceFactory::
          GetDispatcherForBrowserState(browser_state),
      SyncServiceFactory::GetForBrowserState(browser_state),
      SessionSyncServiceFactory::GetForBrowserState(browser_state),
      browser_state_prefs);
}
