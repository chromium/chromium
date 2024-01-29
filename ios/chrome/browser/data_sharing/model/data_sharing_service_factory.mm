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
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
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

  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);
  DCHECK(chrome_browser_state);

  return std::make_unique<DataSharingServiceImpl>(
      browser_state->GetSharedURLLoaderFactory(),
      IdentityManagerFactory::GetForBrowserState(chrome_browser_state));
}

}  // namespace

// static
DataSharingService* DataSharingServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<DataSharingService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
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
