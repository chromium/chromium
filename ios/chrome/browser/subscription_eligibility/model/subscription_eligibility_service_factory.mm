// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/subscription_eligibility/model/subscription_eligibility_service_factory.h"

#import "components/subscription_eligibility/subscription_eligibility_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
subscription_eligibility::SubscriptionEligibilityService*
SubscriptionEligibilityServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<
          subscription_eligibility::SubscriptionEligibilityService>(
          profile, /*create=*/true);
}

// static
SubscriptionEligibilityServiceFactory*
SubscriptionEligibilityServiceFactory::GetInstance() {
  static base::NoDestructor<SubscriptionEligibilityServiceFactory> instance;
  return instance.get();
}

SubscriptionEligibilityServiceFactory::SubscriptionEligibilityServiceFactory()
    : ProfileKeyedServiceFactoryIOS("SubscriptionEligibilityService",
                                    ProfileSelection::kNoInstanceInIncognito) {}

SubscriptionEligibilityServiceFactory::
    ~SubscriptionEligibilityServiceFactory() = default;

std::unique_ptr<KeyedService>
SubscriptionEligibilityServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<
      subscription_eligibility::SubscriptionEligibilityService>(
      profile->GetPrefs());
}
