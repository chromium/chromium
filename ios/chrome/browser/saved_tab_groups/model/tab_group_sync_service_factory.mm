// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"

#import <memory>

#import "components/keyed_service/core/keyed_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/saved_tab_groups/empty_tab_group_store_delegate.h"
#import "components/saved_tab_groups/tab_group_sync_service.h"
#import "components/saved_tab_groups/tab_group_sync_service_impl.h"
#import "components/sync/base/report_unrecoverable_error.h"
#import "components/sync/model/client_tag_based_model_type_processor.h"
#import "components/sync/model/model_type_store_service.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sync/model/model_type_store_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/browser_state.h"

namespace tab_groups {

namespace {
// Returns a configuration for the Saved Tab Group.
std::unique_ptr<TabGroupSyncServiceImpl::SyncDataTypeConfiguration>
CreateSavedTabGroupDataTypeConfiguration(ChromeBrowserState* browser_state) {
  return std::make_unique<TabGroupSyncServiceImpl::SyncDataTypeConfiguration>(
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
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
}

TabGroupSyncServiceFactory::~TabGroupSyncServiceFactory() = default;

std::unique_ptr<KeyedService>
TabGroupSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  auto model = std::make_unique<SavedTabGroupModel>();
  ChromeBrowserState* browser_state = static_cast<ChromeBrowserState*>(context);
  // TODO(b/346624020): Fix dependency for DeviceInfoSyncService and pass
  // metrics logger.
  auto saved_config = CreateSavedTabGroupDataTypeConfiguration(browser_state);

  std::unique_ptr<TabGroupStoreDelegate> tab_group_store_delegate =
      std::make_unique<EmptyTabGroupStoreDelegate>();

  auto tab_group_store =
      std::make_unique<TabGroupStore>(std::move(tab_group_store_delegate));
  std::map<base::Uuid, LocalTabGroupID> migrated_android_local_ids;

  return std::make_unique<TabGroupSyncServiceImpl>(
      std::move(model), std::move(saved_config), nullptr,
      std::move(tab_group_store), browser_state->GetPrefs(),
      std::move(migrated_android_local_ids), /*metrics_logger=*/nullptr);
}

}  // namespace tab_groups
