// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/subscription_eligibility/model/subscription_eligibility_service_factory.h"

#import <utility>

#import "components/subscription_eligibility/subscription_eligibility_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

SubscriptionEligibilityServiceFactory::SubscriptionEligibilityServiceFactory(
    PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "SubscriptionEligibilityService",
          ProfileSelection::kNoInstanceInIncognito) {}

std::unique_ptr<KeyedService>
SubscriptionEligibilityServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<
      subscription_eligibility::SubscriptionEligibilityService>(
      profile->GetPrefs());
}
