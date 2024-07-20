// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/model/plus_address_service_factory.h"

#import <memory>
#import <utility>

#import "base/no_destructor.h"
#import "components/affiliations/core/browser/affiliation_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/plus_addresses/affiliations/plus_address_affiliation_source_adapter.h"
#import "components/plus_addresses/features.h"
#import "components/plus_addresses/plus_address_http_client_impl.h"
#import "components/plus_addresses/plus_address_service.h"
#import "components/variations/service/google_groups_manager.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/metrics/model/google_groups_manager_factory.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_setting_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
plus_addresses::PlusAddressService*
PlusAddressServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<plus_addresses::PlusAddressService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
PlusAddressServiceFactory* PlusAddressServiceFactory::GetInstance() {
  static base::NoDestructor<PlusAddressServiceFactory> instance;
  return instance.get();
}

PlusAddressServiceFactory::PlusAddressServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PlusAddressService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ios::WebDataServiceFactory::GetInstance());
  DependsOn(IOSChromeAffiliationServiceFactory::GetInstance());
  DependsOn(PlusAddressSettingServiceFactory::GetInstance());
  DependsOn(GoogleGroupsManagerFactory::GetInstance());
}

PlusAddressServiceFactory::~PlusAddressServiceFactory() {}

std::unique_ptr<KeyedService>
PlusAddressServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  // If the feature is disabled, don't risk any side effects. Just bail.
  if (!base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressesEnabled)) {
    return nullptr;
  }

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  affiliations::AffiliationService* affiliation_service =
      IOSChromeAffiliationServiceFactory::GetForBrowserState(context);

  // `groups_manager` can be null in tests.
  GoogleGroupsManager* groups_manager =
      GoogleGroupsManagerFactory::GetForBrowserState(browser_state);
  plus_addresses::PlusAddressService::FeatureEnabledForProfileCheck
      feature_check =
          (groups_manager &&
           base::FeatureList::IsEnabled(
               plus_addresses::features::kPlusAddressProfileAwareFeatureCheck))
              ? base::BindRepeating(
                    &GoogleGroupsManager::IsFeatureEnabledForProfile,
                    base::Unretained(groups_manager))
              : base::BindRepeating(&base::FeatureList::IsEnabled);

  std::unique_ptr<plus_addresses::PlusAddressService> plus_address_service =
      std::make_unique<plus_addresses::PlusAddressService>(
          identity_manager,
          PlusAddressSettingServiceFactory::GetForBrowserState(browser_state),
          std::make_unique<plus_addresses::PlusAddressHttpClientImpl>(
              identity_manager, browser_state->GetSharedURLLoaderFactory()),
          ios::WebDataServiceFactory::GetPlusAddressWebDataForBrowserState(
              browser_state, ServiceAccessType::EXPLICIT_ACCESS),
          affiliation_service, std::move(feature_check));

  if (base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressAffiliations)) {
    affiliation_service->RegisterSource(
        std::make_unique<plus_addresses::PlusAddressAffiliationSourceAdapter>(
            plus_address_service.get()));
  }

  return plus_address_service;
}

bool PlusAddressServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

web::BrowserState* PlusAddressServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
