// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace unified_consent {
class UnifiedConsentService;
}

class UnifiedConsentServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static unified_consent::UnifiedConsentService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static unified_consent::UnifiedConsentService* GetForBrowserStateIfExists(
      ChromeBrowserState* browser_state);

  static UnifiedConsentServiceFactory* GetInstance();

  UnifiedConsentServiceFactory(const UnifiedConsentServiceFactory&) = delete;
  UnifiedConsentServiceFactory& operator=(const UnifiedConsentServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<UnifiedConsentServiceFactory>;

  UnifiedConsentServiceFactory();
  ~UnifiedConsentServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_FACTORY_H_
