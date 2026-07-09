// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/personal_context/model/ios_personal_context_eligibility_service_factory.h"

#import <string>

#import "base/no_destructor.h"
#import "base/strings/string_util.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/personal_context/core/country_type.h"
#import "components/personal_context/core/personal_context_eligibility_service_impl.h"
#import "components/personal_context/core/personal_context_features.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/account_settings/model/ios_account_setting_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace {
personal_context::GeoIpCountryCode GetCountryCodeFromVariations() {
  ApplicationContext* application_context = GetApplicationContext();
  variations::VariationsService* variation_service =
      application_context ? application_context->GetVariationsService()
                          : nullptr;
  return personal_context::GeoIpCountryCode(
      variation_service
          ? base::ToUpperASCII(variation_service->GetLatestCountry())
          : std::string());
}
}  // namespace

// static
IOSPersonalContextEligibilityServiceFactory*
IOSPersonalContextEligibilityServiceFactory::GetInstance() {
  static base::NoDestructor<IOSPersonalContextEligibilityServiceFactory>
      instance;
  return instance.get();
}

// static
personal_context::PersonalContextEligibilityService*
IOSPersonalContextEligibilityServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<
          personal_context::PersonalContextEligibilityService>(profile,
                                                               /*create=*/true);
}

IOSPersonalContextEligibilityServiceFactory::
    IOSPersonalContextEligibilityServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PersonalContextEligibilityService",
                                    ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(IOSAccountSettingServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

IOSPersonalContextEligibilityServiceFactory::
    ~IOSPersonalContextEligibilityServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSPersonalContextEligibilityServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(
          personal_context::features::kPersonalContext)) {
    return nullptr;
  }

  account_settings::AccountSettingService* account_settings_service =
      IOSAccountSettingServiceFactory::GetForProfile(
          profile->GetOriginalProfile());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile->GetOriginalProfile());

  ApplicationContext* application_context = GetApplicationContext();
  std::string locale =
      application_context && application_context->GetApplicationLocaleStorage()
          ? application_context->GetApplicationLocaleStorage()->Get()
          : std::string();

  return std::make_unique<
      personal_context::PersonalContextEligibilityServiceImpl>(
      account_settings_service, identity_manager, profile->GetPrefs(),
      GetCountryCodeFromVariations(), locale);
}
