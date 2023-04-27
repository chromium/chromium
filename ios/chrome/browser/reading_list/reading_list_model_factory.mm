// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/reading_list_model_factory.h"

#import <utility>

#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "base/time/default_clock.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/reading_list/core/dual_reading_list_model.h"
#import "components/reading_list/core/reading_list_model_impl.h"
#import "components/reading_list/core/reading_list_model_storage_impl.h"
#import "components/reading_list/features/reading_list_switches.h"
#import "components/sync/base/storage_type.h"
#import "components/sync/model/model_type_store_service.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sync/model_type_store_service_factory.h"
#import "ios/web/public/thread/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
ReadingListModel* ReadingListModelFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<ReadingListModel*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
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
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
}

ReadingListModelFactory::~ReadingListModelFactory() {}

std::unique_ptr<KeyedService> ReadingListModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);

  syncer::OnceModelTypeStoreFactory store_factory =
      ModelTypeStoreServiceFactory::GetForBrowserState(chrome_browser_state)
          ->GetStoreFactory();
  auto storage =
      std::make_unique<ReadingListModelStorageImpl>(std::move(store_factory));
  auto reading_list_model = std::make_unique<ReadingListModelImpl>(
      std::move(storage), syncer::StorageType::kUnspecified,
      base::DefaultClock::GetInstance());
  if (!base::FeatureList::IsEnabled(
          reading_list::switches::kReadingListEnableDualReadingListModel)) {
    return reading_list_model;
  }

  syncer::OnceModelTypeStoreFactory store_factory_for_account_storage =
      ModelTypeStoreServiceFactory::GetForBrowserState(chrome_browser_state)
          ->GetStoreFactoryForAccountStorage();
  auto account_storage = std::make_unique<ReadingListModelStorageImpl>(
      std::move(store_factory_for_account_storage));
  auto reading_list_model_for_account_storage =
      std::make_unique<ReadingListModelImpl>(std::move(account_storage),
                                             syncer::StorageType::kAccount,
                                             base::DefaultClock::GetInstance());
  return std::make_unique<reading_list::DualReadingListModel>(
      /*local_or_syncable_model=*/std::move(reading_list_model),
      /*account_model=*/std::move(reading_list_model_for_account_storage));
}

web::BrowserState* ReadingListModelFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
