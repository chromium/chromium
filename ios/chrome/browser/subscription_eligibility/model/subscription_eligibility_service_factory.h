// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_MODEL_SUBSCRIPTION_ELIGIBILITY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_MODEL_SUBSCRIPTION_ELIGIBILITY_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace subscription_eligibility {
class SubscriptionEligibilityService;
}  // namespace subscription_eligibility

// Singleton that owns all SubscriptionEligibilityServices and associates them
// with ProfileIOS.
class SubscriptionEligibilityServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static subscription_eligibility::SubscriptionEligibilityService*
  GetForProfile(ProfileIOS* profile);

  static SubscriptionEligibilityServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<SubscriptionEligibilityServiceFactory>;

  SubscriptionEligibilityServiceFactory();
  ~SubscriptionEligibilityServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_MODEL_SUBSCRIPTION_ELIGIBILITY_SERVICE_FACTORY_H_
