// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AFFILIATIONS_MODEL_IOS_CHROME_AFFILIATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AFFILIATIONS_MODEL_IOS_CHROME_AFFILIATION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace affiliations {
class AffiliationService;
}

class IOSChromeAffiliationServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static IOSChromeAffiliationServiceFactory* GetInstance();
  static affiliations::AffiliationService* GetForProfile(ProfileIOS* profile);

  // Deprecated: use GetForProfile(...)
  static affiliations::AffiliationService* GetForBrowserState(
      ProfileIOS* profile);

 private:
  friend class base::NoDestructor<IOSChromeAffiliationServiceFactory>;

  IOSChromeAffiliationServiceFactory();
  ~IOSChromeAffiliationServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(web::BrowserState*) const override;
};

#endif  // IOS_CHROME_BROWSER_AFFILIATIONS_MODEL_IOS_CHROME_AFFILIATION_SERVICE_FACTORY_H_
