// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_OHTTP_KEY_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_OHTTP_KEY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class KeyedService;

namespace safe_browsing {
class OhttpKeyService;
}

// Singleton that owns OhttpKeyService objects, one for each active
// BrowserState. It returns nullptr for incognito BrowserStates.
class OhttpKeyServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the instance of OhttpKeyService associated with this profile,
  // creating one if none exists.
  static safe_browsing::OhttpKeyService* GetForProfile(ProfileIOS* profile);

  // Deprecated: use GetForProfile(...).
  static safe_browsing::OhttpKeyService* GetForBrowserState(
      ProfileIOS* profile);

  // Returns the singleton instance of OhttpKeyServiceFactory.
  static OhttpKeyServiceFactory* GetInstance();

  // Returns the default factory. Can be used to force instantiation during
  // testing.
  static TestingFactory GetDefaultFactory();

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

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_OHTTP_KEY_SERVICE_FACTORY_H_
