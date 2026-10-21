// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs_search/model/tabs_search_service_factory.h"

#import <utility>

#import "base/check.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/history/model/web_history_service_factory.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tabs_search/model/tabs_search_service.h"

TabsSearchServiceFactory::TabsSearchServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "TabsSearchService",
          ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(BrowserListFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(IOSChromeTabRestoreServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::WebHistoryServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService> TabsSearchServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  const bool is_off_the_record = profile->IsOffTheRecord();
  syncer::DeviceInfoSyncService* device_info_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);
  return std::make_unique<TabsSearchService>(
      is_off_the_record, BrowserListFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile),
      SyncServiceFactory::GetForProfile(profile),
      IOSChromeTabRestoreServiceFactory::GetForProfile(profile),
      SessionSyncServiceFactory::GetForProfile(profile),
      is_off_the_record ? nullptr
                        : ios::HistoryServiceFactory::GetForProfile(profile),
      is_off_the_record ? nullptr
                        : ios::WebHistoryServiceFactory::GetForProfile(profile),
      device_info_service ? device_info_service->GetDeviceInfoTracker()
                          : nullptr);
}
