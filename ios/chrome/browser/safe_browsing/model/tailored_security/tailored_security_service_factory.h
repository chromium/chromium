// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_FACTORY_H_

#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns TailoredSecurityService objects, one for each active
// profile. It returns nullptr for Incognito profiles.
class TailoredSecurityServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          TailoredSecurityServiceFactory,
          safe_browsing::TailoredSecurityService> {
 public:
  explicit TailoredSecurityServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_TAILORED_SECURITY_TAILORED_SECURITY_SERVICE_FACTORY_H_
