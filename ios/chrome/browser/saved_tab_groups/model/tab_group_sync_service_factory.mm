// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"

#import <algorithm>
#import <memory>

#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/saved_tab_groups/sync_data_type_configuration.h"
#import "components/saved_tab_groups/tab_group_sync_coordinator_impl.h"
#import "components/saved_tab_groups/tab_group_sync_service.h"
#import "components/saved_tab_groups/tab_group_sync_service_impl.h"
#import "components/sync/base/report_unrecoverable_error.h"
#import "components/sync/model/client_tag_based_model_type_processor.h"
#import "components/sync/model/model_type_store_service.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_delegate.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_local_update_observer.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/model_type_store_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/browser_state.h"

namespace tab_groups {

namespace {
// Returns a configuration for the Saved Tab Group.
std::unique_ptr<SyncDataTypeConfiguration>
CreateSavedTabGroupDataTypeConfiguration(ChromeBrowserState* browser_state) {
  return std::make_unique<SyncDataTypeConfiguration>(
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::SAVED_TAB_GROUP,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              ::GetChannel())),
      ModelTypeStoreServiceFactory::GetForBrowserState(browser_state)
          ->GetStoreFactory());
}
}  // namespace

// static
TabGroupSyncService* TabGroupSyncServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<TabGroupSyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state,
                                               /*create=*/true));
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
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(SessionRestorationServiceFactory::GetInstance());
}

TabGroupSyncServiceFactory::~TabGroupSyncServiceFactory() = default;

std::unique_ptr<KeyedService>
TabGroupSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  if (!IsTabGroupSyncEnabled()) {
    return nullptr;
  }

  auto model = std::make_unique<SavedTabGroupModel>();
  ChromeBrowserState* browser_state = static_cast<ChromeBrowserState*>(context);
  CHECK(!browser_state->IsOffTheRecord());
  auto saved_config = CreateSavedTabGroupDataTypeConfiguration(browser_state);

  syncer::DeviceInfoTracker* device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForBrowserState(browser_state)
          ->GetDeviceInfoTracker();
  auto metrics_logger =
      std::make_unique<TabGroupSyncMetricsLogger>(device_info_tracker);

  std::unique_ptr<TabGroupSyncServiceImpl> sync_service =
      std::make_unique<TabGroupSyncServiceImpl>(
          std::move(model), std::move(saved_config), nullptr,
          browser_state->GetPrefs(), std::move(metrics_logger));

  BrowserList* browser_list =
      BrowserListFactory::GetForBrowserState(browser_state);
  std::unique_ptr<TabGroupLocalUpdateObserver> local_update_observer =
      std::make_unique<TabGroupLocalUpdateObserver>(browser_list,
                                                    sync_service.get());

  std::unique_ptr<IOSTabGroupSyncDelegate> delegate =
      std::make_unique<IOSTabGroupSyncDelegate>(
          browser_list, sync_service.get(), std::move(local_update_observer));

  sync_service->SetCoordinator(std::make_unique<TabGroupSyncCoordinatorImpl>(
      std::move(delegate), sync_service.get()));

  return sync_service;
}

}  // namespace tab_groups
