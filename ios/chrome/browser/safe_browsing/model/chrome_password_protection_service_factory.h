// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_PASSWORD_PROTECTION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_PASSWORD_PROTECTION_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class ChromePasswordProtectionService;
class KeyedService;

// Singleton that owns ChromePasswordProtectionService objects, one for each
// active Profile.
class ChromePasswordProtectionServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the instance of ChromePasswordProtectionService associated with
  // this profile, creating one if none exists.
  static ChromePasswordProtectionService* GetForProfile(ProfileIOS* profile);

  // Deprecated: use GetForProfile(...).
  static ChromePasswordProtectionService* GetForBrowserState(
      ProfileIOS* profile);

  // Returns the singleton instance of ChromePasswordProtectionServiceFactory.
  static ChromePasswordProtectionServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ChromePasswordProtectionServiceFactory>;

  ChromePasswordProtectionServiceFactory();
  ~ChromePasswordProtectionServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
  bool ServiceIsCreatedWithBrowserState() const override;
  web::BrowserState* GetBrowserStateToUse(web::BrowserState*) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CHROME_PASSWORD_PROTECTION_SERVICE_FACTORY_H_
