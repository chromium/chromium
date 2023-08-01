// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_OHTTP_KEY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_OHTTP_KEY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class KeyedService;

namespace safe_browsing {
class OhttpKeyService;
}

namespace web {
class BrowserState;
}

// Singleton that owns OhttpKeyService objects, one for each active
// BrowserState. It returns nullptr for incognito BrowserStates.
class OhttpKeyServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the instance of OhttpKeyService associated with this browser state,
  // creating one if none exists.
  static safe_browsing::OhttpKeyService* GetForBrowserState(
      web::BrowserState* browser_state);

  // Returns the singleton instance of OhttpKeyServiceFactory.
  static OhttpKeyServiceFactory* GetInstance();

  OhttpKeyServiceFactory(const OhttpKeyServiceFactory&) = delete;
  OhttpKeyServiceFactory& operator=(const OhttpKeyServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<OhttpKeyServiceFactory>;

  OhttpKeyServiceFactory();
  ~OhttpKeyServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
  bool ServiceIsCreatedWithBrowserState() const override;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_OHTTP_KEY_SERVICE_FACTORY_H_
