// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"

#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/browser_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
AimEligibilityService* IOSChromeAimEligibilityServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<IOSChromeAimEligibilityService>(
      profile, /*create=*/true);
}

// static
IOSChromeAimEligibilityServiceFactory*
IOSChromeAimEligibilityServiceFactory::GetInstance() {
  static base::NoDestructor<IOSChromeAimEligibilityServiceFactory> instance;
  return instance.get();
}

IOSChromeAimEligibilityServiceFactory::IOSChromeAimEligibilityServiceFactory()
    : ProfileKeyedServiceFactoryIOS(
          "AimEligibilityService",
          IsAIMEligibilityServiceStartWithProfileEnabled()
              ? ServiceCreation::kCreateWithProfile
              : ServiceCreation::kDefault,
          ProfileSelection::kOwnInstanceInIncognito,
          IsAIMEligibilityServiceStartWithProfileEnabled()
              ? TestingCreation::kNoServiceForTests
              : TestingCreation::kDefault) {
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

IOSChromeAimEligibilityServiceFactory::
    ~IOSChromeAimEligibilityServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSChromeAimEligibilityServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<IOSChromeAimEligibilityService>(
      profile->GetPrefs(),
      ios::TemplateURLServiceFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory(),
      IdentityManagerFactory::GetForProfile(profile));
}
