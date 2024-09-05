// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIFIED_CONSENT_MODEL_UNIFIED_CONSENT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_UNIFIED_CONSENT_MODEL_UNIFIED_CONSENT_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace unified_consent {
class UnifiedConsentService;
}

class UnifiedConsentServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static unified_consent::UnifiedConsentService* GetForProfile(
      ProfileIOS* profile);

  static unified_consent::UnifiedConsentService* GetForProfileIfExists(
      ProfileIOS* profile);

  static UnifiedConsentServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<UnifiedConsentServiceFactory>;

  UnifiedConsentServiceFactory();
  ~UnifiedConsentServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_UNIFIED_CONSENT_MODEL_UNIFIED_CONSENT_SERVICE_FACTORY_H_
