// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_OHTTP_KEY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_OHTTP_KEY_SERVICE_FACTORY_H_

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
  bool ServiceIsNULLWhileTesting() const override;
};

// Used only for tests. By default, the OHTTP key service is null for tests,
// since when it's created it tries to fetch the OHTTP key, which can cause
// errors for unrelated tests. To allow the OHTTP key service in tests, create
// an object of this type and keep it in scope for as long as the override
// should exist. The constructor will set the override, and the destructor will
// clear it.
class OhttpKeyServiceAllowerForTesting {
 public:
  OhttpKeyServiceAllowerForTesting();
  OhttpKeyServiceAllowerForTesting(const OhttpKeyServiceAllowerForTesting&) =
      delete;
  OhttpKeyServiceAllowerForTesting& operator=(
      const OhttpKeyServiceAllowerForTesting&) = delete;
  ~OhttpKeyServiceAllowerForTesting();
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_OHTTP_KEY_SERVICE_FACTORY_H_
