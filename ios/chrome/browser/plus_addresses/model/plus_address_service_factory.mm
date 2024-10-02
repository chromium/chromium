// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/model/plus_address_service_factory.h"

#import <memory>
#import <utility>

#import "base/no_destructor.h"
#import "components/affiliations/core/browser/affiliation_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/plus_addresses/affiliations/plus_address_affiliation_source_adapter.h"
#import "components/plus_addresses/features.h"
#import "components/plus_addresses/plus_address_http_client_impl.h"
#import "components/plus_addresses/plus_address_service_impl.h"
#import "components/variations/service/google_groups_manager.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/metrics/model/google_groups_manager_factory.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_setting_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
plus_addresses::PlusAddressService* PlusAddressServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<plus_addresses::PlusAddressService>(profile,
                                                                   true);
}

// static
PlusAddressServiceFactory* PlusAddressServiceFactory::GetInstance() {
  static base::NoDestructor<PlusAddressServiceFactory> instance;
  return instance.get();
}

PlusAddressServiceFactory::PlusAddressServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PlusAddressService",
                                    ProfileSelection::kRedirectedInIncognito,
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
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

  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  affiliations::AffiliationService* affiliation_service =
      IOSChromeAffiliationServiceFactory::GetForBrowserState(profile);

  // `groups_manager` can be null in tests.
  GoogleGroupsManager* groups_manager =
      GoogleGroupsManagerFactory::GetForProfile(profile);
  plus_addresses::PlusAddressServiceImpl::FeatureEnabledForProfileCheck
      feature_check =
          (groups_manager &&
           base::FeatureList::IsEnabled(
               plus_addresses::features::kPlusAddressProfileAwareFeatureCheck))
              ? base::BindRepeating(
                    &GoogleGroupsManager::IsFeatureEnabledForProfile,
                    base::Unretained(groups_manager))
              : base::BindRepeating(&base::FeatureList::IsEnabled);

  if (auto test_service = tests_hook::GetOverriddenPlusAddressService()) {
    return test_service;
  }

  std::unique_ptr<plus_addresses::PlusAddressServiceImpl> plus_address_service =
      std::make_unique<plus_addresses::PlusAddressServiceImpl>(
          profile->GetPrefs(), identity_manager,
          PlusAddressSettingServiceFactory::GetForProfile(profile),
          std::make_unique<plus_addresses::PlusAddressHttpClientImpl>(
              identity_manager, profile->GetSharedURLLoaderFactory()),
          ios::WebDataServiceFactory::GetPlusAddressWebDataForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS),
          affiliation_service, std::move(feature_check));

  affiliation_service->RegisterSource(
      std::make_unique<plus_addresses::PlusAddressAffiliationSourceAdapter>(
          plus_address_service.get()));
  return plus_address_service;
}
