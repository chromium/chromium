// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/ios_autofill_ai_personal_context_access_manager_factory.h"

#import "base/feature_list.h"
#import "components/autofill/core/browser/network/autofill_ai/autofill_ai_personal_context_access_manager_impl.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/personal_context/core/personal_context_eligibility_service.h"
#import "components/personal_context/core/personal_context_service.h"
#import "ios/chrome/browser/personal_context/model/ios_personal_context_eligibility_service_factory.h"
#import "ios/chrome/browser/personal_context/model/ios_personal_context_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
autofill::AutofillAiPersonalContextAccessManager*
IOSAutofillAiPersonalContextAccessManagerFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<
          autofill::AutofillAiPersonalContextAccessManager>(profile, true);
}

// static
IOSAutofillAiPersonalContextAccessManagerFactory*
IOSAutofillAiPersonalContextAccessManagerFactory::GetInstance() {
  static base::NoDestructor<IOSAutofillAiPersonalContextAccessManagerFactory>
      instance;
  return instance.get();
}

IOSAutofillAiPersonalContextAccessManagerFactory::
    IOSAutofillAiPersonalContextAccessManagerFactory()
    : ProfileKeyedServiceFactoryIOS("AutofillAiPersonalContextAccessManager",
                                    ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(IOSPersonalContextEligibilityServiceFactory::GetInstance());
  DependsOn(IOSPersonalContextServiceFactory::GetInstance());
}

IOSAutofillAiPersonalContextAccessManagerFactory::
    ~IOSAutofillAiPersonalContextAccessManagerFactory() = default;

std::unique_ptr<KeyedService>
IOSAutofillAiPersonalContextAccessManagerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAmbientAutofill)) {
    return nullptr;
  }

  personal_context::PersonalContextService* personal_context_service =
      IOSPersonalContextServiceFactory::GetForProfile(profile);
  personal_context::PersonalContextEligibilityService*
      personal_context_eligibility_service =
          IOSPersonalContextEligibilityServiceFactory::GetForProfile(profile);

  if (!personal_context_service || !personal_context_eligibility_service) {
    return nullptr;
  }

  return std::make_unique<autofill::AutofillAiPersonalContextAccessManagerImpl>(
      personal_context_service, personal_context_eligibility_service,
      profile->GetPrefs());
}
