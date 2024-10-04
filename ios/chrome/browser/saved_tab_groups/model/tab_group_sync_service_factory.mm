// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"

#import <algorithm>
#import <memory>

#import "components/data_sharing/public/features.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "components/saved_tab_groups/public/tab_group_sync_service_factory_helper.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_delegate.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_local_update_observer.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/browser_state.h"

namespace tab_groups {

// static
TabGroupSyncService* TabGroupSyncServiceFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
TabGroupSyncService* TabGroupSyncServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<TabGroupSyncService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

TabGroupSyncServiceFactory* TabGroupSyncServiceFactory::GetInstance() {
  static base::NoDestructor<TabGroupSyncServiceFactory> instance;
  return instance.get();
}

TabGroupSyncServiceFactory::TabGroupSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "TabGroupSyncServiceFactory",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(BrowserListFactory::GetInstance());
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(SessionRestorationServiceFactory::GetInstance());
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
  // The dependency on IdentityManager is only for the purpose of recording "on
  // signin" metrics.
  DependsOn(IdentityManagerFactory::GetInstance());
}

TabGroupSyncServiceFactory::~TabGroupSyncServiceFactory() = default;

std::unique_ptr<KeyedService>
TabGroupSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  if (!IsTabGroupSyncEnabled()) {
    return nullptr;
  }

  ProfileIOS* profile = static_cast<ProfileIOS*>(context);
  CHECK(!profile->IsOffTheRecord());

  // Give the opportunity for the test hook to override the factory from
  // the provider (allowing EG tests to use a fake TabGroupSyncService).
  if (auto sync_service = tests_hook::CreateTabGroupSyncService(profile)) {
    return sync_service;
  }

  syncer::DeviceInfoTracker* device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();
  auto* opt_guide = OptimizationGuideServiceFactory::GetForProfile(profile);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

  std::unique_ptr<TabGroupSyncService> sync_service = CreateTabGroupSyncService(
      ::GetChannel(), DataTypeStoreServiceFactory::GetForProfile(profile),
      profile->GetPrefs(), device_info_tracker, opt_guide, identity_manager);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);
  std::unique_ptr<TabGroupLocalUpdateObserver> local_update_observer =
      std::make_unique<TabGroupLocalUpdateObserver>(browser_list,
                                                    sync_service.get());

  std::unique_ptr<IOSTabGroupSyncDelegate> delegate =
      std::make_unique<IOSTabGroupSyncDelegate>(
          browser_list, sync_service.get(), std::move(local_update_observer));

  sync_service->SetTabGroupSyncDelegate(std::move(delegate));
  return sync_service;
}

}  // namespace tab_groups
