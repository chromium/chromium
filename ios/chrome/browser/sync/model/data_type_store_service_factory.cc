// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/model/data_type_store_service_factory.h"

#include "components/sync/model/data_type_store_service_impl.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
DataTypeStoreServiceFactory* DataTypeStoreServiceFactory::GetInstance() {
  static base::NoDestructor<DataTypeStoreServiceFactory> instance;
  return instance.get();
}

// static
syncer::DataTypeStoreService* DataTypeStoreServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<syncer::DataTypeStoreService>(
      profile, /*create=*/true);
}

DataTypeStoreServiceFactory::DataTypeStoreServiceFactory()
    : ProfileKeyedServiceFactoryIOS("DataTypeStoreService",
                                    ProfileSelection::kRedirectedInIncognito) {}

DataTypeStoreServiceFactory::~DataTypeStoreServiceFactory() = default;

std::unique_ptr<KeyedService>
DataTypeStoreServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<syncer::DataTypeStoreServiceImpl>(
      profile->GetStatePath(), profile->GetPrefs());
}
