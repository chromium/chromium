// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"

#import <memory>

#import "base/check_deref.h"
#import "base/strings/string_util.h"
#import "components/country_codes/country_codes.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "components/regional_capabilities/regional_capabilities_utils.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

namespace {

class RegionalCapabilitiesServiceClient
    : public regional_capabilities::RegionalCapabilitiesService::Client {
 public:
  explicit RegionalCapabilitiesServiceClient(
      variations::VariationsService* variations_service)
      : variations_latest_country_id_(
            variations_service ? country_codes::CountryId(base::ToUpperASCII(
                                     variations_service->GetLatestCountry()))
                               : country_codes::CountryId()) {}

  country_codes::CountryId GetVariationsLatestCountryId() override {
    return variations_latest_country_id_;
  }

  country_codes::CountryId GetFallbackCountryId() override {
    return country_codes::GetCurrentCountryID();
  }

  void FetchCountryId(CountryIdCallback country_id_fetched_callback) override {
    std::move(country_id_fetched_callback)
        .Run(country_codes::GetCurrentCountryID());
  }

 private:
  country_codes::CountryId variations_latest_country_id_;
};

}  // namespace

RegionalCapabilitiesServiceFactory::RegionalCapabilitiesServiceFactory()
    : ProfileKeyedServiceFactoryIOS("RegionalCapabilitiesService",
                                    ProfileSelection::kRedirectedInIncognito) {}

RegionalCapabilitiesServiceFactory::~RegionalCapabilitiesServiceFactory() =
    default;

// static
RegionalCapabilitiesServiceFactory*
RegionalCapabilitiesServiceFactory::GetInstance() {
  static base::NoDestructor<RegionalCapabilitiesServiceFactory> factory;
  return factory.get();
}

// static
regional_capabilities::RegionalCapabilitiesService*
RegionalCapabilitiesServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<
          regional_capabilities::RegionalCapabilitiesService>(profile,
                                                              /*create=*/true);
}

std::unique_ptr<KeyedService>
RegionalCapabilitiesServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<regional_capabilities::RegionalCapabilitiesService>(
      CHECK_DEREF(profile->GetPrefs()),
      std::make_unique<RegionalCapabilitiesServiceClient>(
          GetApplicationContext()->GetVariationsService()));
}

}  // namespace ios
