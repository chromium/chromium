// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/ios_personal_context_enablement_service_factory.h"

#import "base/feature_list.h"
#import "base/strings/string_util.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/personal_context/core/personal_context_enablement_service_impl.h"
#import "components/personal_context/core/personal_context_features.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/account_settings/model/ios_account_setting_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

namespace personal_context {

namespace {
GeoIpCountryCode GetCountryCodeFromVariations() {
  variations::VariationsService* variation_service =
      GetApplicationContext()->GetVariationsService();
  return GeoIpCountryCode(
      variation_service
          ? base::ToUpperASCII(variation_service->GetLatestCountry())
          : std::string());
}
}  // namespace

PersonalContextEnablementService*
IOSPersonalContextEnablementServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<PersonalContextEnablementService>(profile, true);
}

// static
IOSPersonalContextEnablementServiceFactory*
IOSPersonalContextEnablementServiceFactory::GetInstance() {
  static base::NoDestructor<IOSPersonalContextEnablementServiceFactory>
      instance;
  return instance.get();
}

IOSPersonalContextEnablementServiceFactory::
    IOSPersonalContextEnablementServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PersonalContextEnablementService",
                                    ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(IOSAccountSettingServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

IOSPersonalContextEnablementServiceFactory::
    ~IOSPersonalContextEnablementServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSPersonalContextEnablementServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(features::kPersonalContext)) {
    return nullptr;
  }

  account_settings::AccountSettingService* account_settings_service =
      IOSAccountSettingServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return std::make_unique<PersonalContextEnablementServiceImpl>(
      account_settings_service, identity_manager, profile->GetPrefs(),
      GetCountryCodeFromVariations(),
      GetApplicationContext()->GetApplicationLocaleStorage()->Get());
}

}  // namespace personal_context
