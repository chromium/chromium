// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/personal_context/model/ios_personal_context_first_run_service_factory.h"

#import "base/no_destructor.h"
#import "components/personal_context/first_run/personal_context_first_run_service_impl.h"
#import "ios/chrome/browser/personal_context/model/ios_personal_context_eligibility_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

// static
IOSPersonalContextFirstRunServiceFactory*
IOSPersonalContextFirstRunServiceFactory::GetInstance() {
  static base::NoDestructor<IOSPersonalContextFirstRunServiceFactory> instance;
  return instance.get();
}

// static
personal_context::PersonalContextFirstRunService*
IOSPersonalContextFirstRunServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<
          personal_context::PersonalContextFirstRunService>(profile,
                                                            /*create=*/true);
}

IOSPersonalContextFirstRunServiceFactory::
    IOSPersonalContextFirstRunServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PersonalContextFirstRunService",
                                    ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(IOSPersonalContextEligibilityServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

IOSPersonalContextFirstRunServiceFactory::
    ~IOSPersonalContextFirstRunServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSPersonalContextFirstRunServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  personal_context::PersonalContextEligibilityService* eligibility_service =
      IOSPersonalContextEligibilityServiceFactory::GetForProfile(profile);
  if (!eligibility_service) {
    return nullptr;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  return std::make_unique<personal_context::PersonalContextFirstRunServiceImpl>(
      eligibility_service, profile->GetPrefs(), identity_manager);
}
