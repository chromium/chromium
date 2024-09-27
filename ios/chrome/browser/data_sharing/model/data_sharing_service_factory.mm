// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"

#import <memory>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "components/data_sharing/internal/data_sharing_service_impl.h"
#import "components/data_sharing/internal/empty_data_sharing_service.h"
#import "components/data_sharing/public/data_sharing_service.h"
#import "components/data_sharing/public/features.h"
#import "components/keyed_service/core/keyed_service_export.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/sync/model/data_type_store_service.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_sdk_delegate_ios.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_ui_delegate_ios.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/browser_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace data_sharing {
namespace {

std::unique_ptr<KeyedService> BuildDataSharingService(
    web::BrowserState* browser_state) {
  if (!browser_state) {
    return nullptr;
  }

  if (!base::FeatureList::IsEnabled(features::kDataSharingFeature) ||
      browser_state->IsOffTheRecord()) {
    return std::make_unique<EmptyDataSharingService>();
  }

  ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
  DCHECK(profile);

  std::unique_ptr<DataSharingUIDelegate> ui_delegate =
      std::make_unique<DataSharingUIDelegateIOS>();
  std::unique_ptr<DataSharingSDKDelegate> sdk_delegate =
      std::make_unique<DataSharingSDKDelegateIOS>();

  return std::make_unique<DataSharingServiceImpl>(
      browser_state->GetSharedURLLoaderFactory(),
      IdentityManagerFactory::GetForProfile(profile),
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
      ::GetChannel(), std::move(sdk_delegate), std::move(ui_delegate));
}

}  // namespace

// static
DataSharingService* DataSharingServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<DataSharingService*>(
      GetInstance()->GetServiceForBrowserState(profile, /*create=*/true));
}

// static
DataSharingServiceFactory* DataSharingServiceFactory::GetInstance() {
  static base::NoDestructor<DataSharingServiceFactory> instance;
  return instance.get();
}

DataSharingServiceFactory::DataSharingServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "DataSharingService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

DataSharingServiceFactory::~DataSharingServiceFactory() = default;

// static
BrowserStateKeyedServiceFactory::TestingFactory
DataSharingServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildDataSharingService);
}

std::unique_ptr<KeyedService>
DataSharingServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  return BuildDataSharingService(browser_state);
}

web::BrowserState* DataSharingServiceFactory::GetBrowserStateToUse(
    web::BrowserState* browser_state) const {
  return GetBrowserStateOwnInstanceInIncognito(browser_state);
}

}  // namespace data_sharing
