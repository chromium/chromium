// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "components/data_sharing/internal/data_sharing_service_impl.h"
#import "components/data_sharing/internal/empty_data_sharing_service.h"
#import "components/data_sharing/public/data_sharing_service.h"
#import "components/data_sharing/public/features.h"
#import "components/sync/model/data_type_store_service.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace data_sharing {

namespace {

std::unique_ptr<KeyedService> BuildDataSharingService(ProfileIOS* profile) {
  if (!features::IsDataSharingFunctionalityEnabled() ||
      profile->IsOffTheRecord()) {
    return std::make_unique<EmptyDataSharingService>();
  }

  auto data_sharing_service = std::make_unique<DataSharingServiceImpl>(
      profile->GetStatePath(), profile->GetSharedURLLoaderFactory(),
      IdentityManagerFactory::GetForProfile(profile),
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
      ::GetChannel(), /* sdk_delegate = */ nullptr,
      /* ui_delegate = */ nullptr);

  tests_hook::DataSharingServiceHooks(data_sharing_service.get());

  return data_sharing_service;
}

}  // namespace

// static
DataSharingService* DataSharingServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<DataSharingService>(
      profile, /*create=*/true);
}

// static
DataSharingServiceFactory* DataSharingServiceFactory::GetInstance() {
  static base::NoDestructor<DataSharingServiceFactory> instance;
  return instance.get();
}

DataSharingServiceFactory::DataSharingServiceFactory()
    : ProfileKeyedServiceFactoryIOS("DataSharingService",
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

DataSharingServiceFactory::~DataSharingServiceFactory() = default;

// static
DataSharingServiceFactory::TestingFactory
DataSharingServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildDataSharingService);
}

std::unique_ptr<KeyedService>
DataSharingServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return BuildDataSharingService(profile);
}

}  // namespace data_sharing
