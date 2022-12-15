// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/reading_list_model_factory.h"

#import <utility>

#import "base/bind.h"
#import "base/files/file_path.h"
#import "base/no_destructor.h"
#import "base/time/default_clock.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/reading_list/core/reading_list_model_impl.h"
#import "components/reading_list/core/reading_list_model_storage_impl.h"
#import "components/reading_list/core/reading_list_pref_names.h"
#import "components/sync/model/model_type_store_service.h"
#import "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/flags/system_flags.h"
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

void ReadingListModelFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      reading_list::prefs::kDeprecatedReadingListHasUnseenEntries, false,
      PrefRegistry::NO_REGISTRATION_FLAGS);
}

std::unique_ptr<KeyedService> ReadingListModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(context);

  syncer::OnceModelTypeStoreFactory store_factory =
      ModelTypeStoreServiceFactory::GetForBrowserState(chrome_browser_state)
          ->GetStoreFactory();
  auto storage =
      std::make_unique<ReadingListModelStorageImpl>(std::move(store_factory));
  std::unique_ptr<KeyedService> reading_list_model =
      std::make_unique<ReadingListModelImpl>(std::move(storage),
                                             base::DefaultClock::GetInstance());
  return reading_list_model;
}

web::BrowserState* ReadingListModelFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
