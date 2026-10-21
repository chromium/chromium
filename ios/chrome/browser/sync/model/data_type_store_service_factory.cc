// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/model/data_type_store_service_factory.h"

#include <utility>

#include "components/sync/model/data_type_store_service_impl.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

DataTypeStoreServiceFactory::DataTypeStoreServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "DataTypeStoreService",
          ProfileSelection::kRedirectedInIncognito) {}

std::unique_ptr<KeyedService>
DataTypeStoreServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<syncer::DataTypeStoreServiceImpl>(
      profile->GetStatePath(), profile->GetPrefs());
}
