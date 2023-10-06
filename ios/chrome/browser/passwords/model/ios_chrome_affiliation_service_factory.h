// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_AFFILIATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_AFFILIATION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"

namespace password_manager {
class AffiliationService;
}

class IOSChromeAffiliationServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static IOSChromeAffiliationServiceFactory* GetInstance();
  static password_manager::AffiliationService* GetForBrowserState(
      web::BrowserState* browser_state);

 private:
  friend class base::NoDestructor<IOSChromeAffiliationServiceFactory>;

  IOSChromeAffiliationServiceFactory();
  ~IOSChromeAffiliationServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_AFFILIATION_SERVICE_FACTORY_H_
