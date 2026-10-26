// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_MODEL_IOS_CHROME_AIM_ELIGIBILITY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AIM_MODEL_IOS_CHROME_AIM_ELIGIBILITY_SERVICE_FACTORY_H_

#include "components/omnibox/browser/aim_eligibility_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all IOSChromeAimEligibilityService and associates them
// with ProfileIOS.
class IOSChromeAimEligibilityServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSChromeAimEligibilityServiceFactory,
          AimEligibilityService> {
 public:
  explicit IOSChromeAimEligibilityServiceFactory(PassKey key);

  // Returns the default factory used to build IOSChromeAimEligibilityService.
  // Can be registered with AddTestingFactory to use real instances during
  // testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_AIM_MODEL_IOS_CHROME_AIM_ELIGIBILITY_SERVICE_FACTORY_H_
