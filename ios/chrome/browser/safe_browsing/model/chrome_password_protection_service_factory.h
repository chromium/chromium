// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_PASSWORD_PROTECTION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_PASSWORD_PROTECTION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ChromePasswordProtectionService;
class KeyedService;
class ProfileIOS;

// Singleton that owns ChromePasswordProtectionService objects, one for each
// active Profile.
class ChromePasswordProtectionServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the instance of ChromePasswordProtectionService associated with
  // this profile, creating one if none exists.
  static ChromePasswordProtectionService* GetForProfile(ProfileIOS* profile);

  // Returns the singleton instance of ChromePasswordProtectionServiceFactory.
  static ChromePasswordProtectionServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ChromePasswordProtectionServiceFactory>;

  ChromePasswordProtectionServiceFactory();
  ~ChromePasswordProtectionServiceFactory() override = default;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_PASSWORD_PROTECTION_SERVICE_FACTORY_H_
