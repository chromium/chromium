// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_MODEL_IOS_CHROME_AIM_ELIGIBILITY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AIM_MODEL_IOS_CHROME_AIM_ELIGIBILITY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;
class AimEligibilityService;

// Singleton that owns all IOSChromeAimEligibilityService and associates them
// with ProfileIOS.
class IOSChromeAimEligibilityServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static AimEligibilityService* GetForProfile(ProfileIOS* profile);

  static IOSChromeAimEligibilityServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSChromeAimEligibilityServiceFactory>;

  IOSChromeAimEligibilityServiceFactory();
  ~IOSChromeAimEligibilityServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_AIM_MODEL_IOS_CHROME_AIM_ELIGIBILITY_SERVICE_FACTORY_H_
