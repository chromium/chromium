// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AFFILIATIONS_MODEL_IOS_CHROME_AFFILIATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AFFILIATIONS_MODEL_IOS_CHROME_AFFILIATION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace affiliations {
class AffiliationService;
}

class IOSChromeAffiliationServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static IOSChromeAffiliationServiceFactory* GetInstance();
  static affiliations::AffiliationService* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<IOSChromeAffiliationServiceFactory>;

  IOSChromeAffiliationServiceFactory();
  ~IOSChromeAffiliationServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_AFFILIATIONS_MODEL_IOS_CHROME_AFFILIATION_SERVICE_FACTORY_H_
