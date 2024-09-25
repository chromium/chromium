// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/model/data_type_store_service_factory.h"

#include <utility>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sync/model/data_type_store_service_impl.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
DataTypeStoreServiceFactory* DataTypeStoreServiceFactory::GetInstance() {
  static base::NoDestructor<DataTypeStoreServiceFactory> instance;
  return instance.get();
}

// static
syncer::DataTypeStoreService* DataTypeStoreServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<syncer::DataTypeStoreService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

DataTypeStoreServiceFactory::DataTypeStoreServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "DataTypeStoreService",
          BrowserStateDependencyManager::GetInstance()) {}

DataTypeStoreServiceFactory::~DataTypeStoreServiceFactory() {}

std::unique_ptr<KeyedService>
DataTypeStoreServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<syncer::DataTypeStoreServiceImpl>(
      profile->GetStatePath(), profile->GetPrefs());
}

web::BrowserState* DataTypeStoreServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
