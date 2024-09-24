// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"

#import <utility>

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "base/time/default_clock.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/reading_list/core/dual_reading_list_model.h"
#import "components/reading_list/core/reading_list_model_impl.h"
#import "components/reading_list/core/reading_list_model_storage_impl.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/base/storage_type.h"
#import "components/sync/model/data_type_store_service.h"
#import "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/web/public/thread/web_thread.h"

namespace {

// Returns what the local-or-syncable instance should do when sync is disabled,
// that is, whether reading list entries might need to be deleted.
syncer::WipeModelUponSyncDisabledBehavior
GetWipeModelUponSyncDisabledBehaviorForSyncableModel() {
  if (IsFirstSessionAfterDeviceRestore() != signin::Tribool::kTrue) {
    return syncer::WipeModelUponSyncDisabledBehavior::kNever;
  }

  return syncer::WipeModelUponSyncDisabledBehavior::kOnceIfTrackingMetadata;
}

}  // namespace

// static
ReadingListModel* ReadingListModelFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
ReadingListModel* ReadingListModelFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<ReadingListModel*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
reading_list::DualReadingListModel*
ReadingListModelFactory::GetAsDualReadingListModelForProfile(
    ProfileIOS* profile) {
  return static_cast<reading_list::DualReadingListModel*>(
      GetForProfile(profile));
}

// static
ReadingListModelFactory* ReadingListModelFactory::GetInstance() {
  static base::NoDestructor<ReadingListModelFactory> instance;
  return instance.get();
}

ReadingListModelFactory::ReadingListModelFactory()
    : BrowserStateKeyedServiceFactory(
          "ReadingListModel",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
}

ReadingListModelFactory::~ReadingListModelFactory() {}

std::unique_ptr<KeyedService> ReadingListModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  syncer::OnceDataTypeStoreFactory store_factory =
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();
  auto local_storage =
      std::make_unique<ReadingListModelStorageImpl>(std::move(store_factory));
  auto reading_list_model_for_local_storage =
      std::make_unique<ReadingListModelImpl>(
          std::move(local_storage), syncer::StorageType::kUnspecified,
          GetWipeModelUponSyncDisabledBehaviorForSyncableModel(),
          base::DefaultClock::GetInstance());

  syncer::OnceDataTypeStoreFactory store_factory_for_account_storage =
      DataTypeStoreServiceFactory::GetForProfile(profile)
          ->GetStoreFactoryForAccountStorage();
  auto account_storage = std::make_unique<ReadingListModelStorageImpl>(
      std::move(store_factory_for_account_storage));
  auto reading_list_model_for_account_storage =
      std::make_unique<ReadingListModelImpl>(
          std::move(account_storage), syncer::StorageType::kAccount,
          syncer::WipeModelUponSyncDisabledBehavior::kAlways,
          base::DefaultClock::GetInstance());
  return std::make_unique<reading_list::DualReadingListModel>(
      /*local_or_syncable_model=*/std::move(
          reading_list_model_for_local_storage),
      /*account_model=*/std::move(reading_list_model_for_account_storage));
}

web::BrowserState* ReadingListModelFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
