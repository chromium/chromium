// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_PASSWORD_PROTECTION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_PASSWORD_PROTECTION_SERVICE_FACTORY_H_

#include "ios/chrome/browser/safe_browsing/model/chrome_password_protection_service.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns ChromePasswordProtectionService objects, one for each
// active Profile.
class ChromePasswordProtectionServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          ChromePasswordProtectionServiceFactory,
          ChromePasswordProtectionService> {
 public:
  explicit ChromePasswordProtectionServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_PASSWORD_PROTECTION_SERVICE_FACTORY_H_
