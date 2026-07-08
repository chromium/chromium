// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/personal_context/model/ios_personal_context_access_manager_factory.h"

#import "base/feature_list.h"
#import "components/autofill/core/browser/network/autofill_ai/personal_context_access_manager_impl.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/personal_context/core/personal_context_enablement_service.h"
#import "components/personal_context/core/personal_context_service.h"
#import "ios/chrome/browser/personal_context/model/ios_personal_context_enablement_service_factory.h"
#import "ios/chrome/browser/personal_context/model/ios_personal_context_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
autofill::PersonalContextAccessManager*
IOSPersonalContextAccessManagerFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<autofill::PersonalContextAccessManager>(profile,
                                                                       true);
}

// static
IOSPersonalContextAccessManagerFactory*
IOSPersonalContextAccessManagerFactory::GetInstance() {
  static base::NoDestructor<IOSPersonalContextAccessManagerFactory> instance;
  return instance.get();
}

IOSPersonalContextAccessManagerFactory::IOSPersonalContextAccessManagerFactory()
    : ProfileKeyedServiceFactoryIOS("PersonalContextAccessManager",
                                    ProfileSelection::kNoInstanceInIncognito) {
  DependsOn(IOSPersonalContextEnablementServiceFactory::GetInstance());
  DependsOn(IOSPersonalContextServiceFactory::GetInstance());
}

IOSPersonalContextAccessManagerFactory::
    ~IOSPersonalContextAccessManagerFactory() = default;

std::unique_ptr<KeyedService>
IOSPersonalContextAccessManagerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAmbientAutofill)) {
    return nullptr;
  }

  personal_context::PersonalContextService* personal_context_service =
      IOSPersonalContextServiceFactory::GetForProfile(profile);
  personal_context::PersonalContextEnablementService*
      personal_context_enablement_service =
          IOSPersonalContextEnablementServiceFactory::GetForProfile(profile);

  if (!personal_context_service || !personal_context_enablement_service) {
    return nullptr;
  }

  return std::make_unique<autofill::PersonalContextAccessManagerImpl>(
      personal_context_service, personal_context_enablement_service,
      profile->GetPrefs());
}
