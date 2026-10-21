// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PERSONAL_CONTEXT_MODEL_IOS_PERSONAL_CONTEXT_ELIGIBILITY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PERSONAL_CONTEXT_MODEL_IOS_PERSONAL_CONTEXT_ELIGIBILITY_SERVICE_FACTORY_H_

#import "components/personal_context/core/personal_context_eligibility_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// KeyedServiceFactory for PersonalContextEligibilityService on iOS.
class IOSPersonalContextEligibilityServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSPersonalContextEligibilityServiceFactory,
          personal_context::PersonalContextEligibilityService> {
 public:
  explicit IOSPersonalContextEligibilityServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PERSONAL_CONTEXT_MODEL_IOS_PERSONAL_CONTEXT_ELIGIBILITY_SERVICE_FACTORY_H_
