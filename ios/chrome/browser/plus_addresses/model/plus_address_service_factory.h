// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PLUS_ADDRESSES_MODEL_PLUS_ADDRESS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PLUS_ADDRESSES_MODEL_PLUS_ADDRESS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace plus_addresses {
class PlusAddressService;
}

// A `BrowserStateKeyedServiceFactory` implementation for offering
// plus_addresses in autofill. Comparable in function to the
// plus_address_service_factory in //chrome/browser/plus_addresses.
class PlusAddressServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static plus_addresses::PlusAddressService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static PlusAddressServiceFactory* GetInstance();

  PlusAddressServiceFactory(const PlusAddressServiceFactory&) = delete;
  PlusAddressServiceFactory& operator=(const PlusAddressServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<PlusAddressServiceFactory>;
  PlusAddressServiceFactory();
  ~PlusAddressServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  // The service must be created with the browser state, such that data can be
  // loaded prior to the first use in an autofill flow.
  bool ServiceIsCreatedWithBrowserState() const override;

  // The service is intentionally null when the base::Feature is disabled.
  bool ServiceIsNULLWhileTesting() const override;

  // Ensure that the service is available in incognito mode. Existing
  // plus_addresses are still offered in that mode, while creation of new ones
  // is disabled in the PlusAddressService implementation.
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PLUS_ADDRESSES_MODEL_PLUS_ADDRESS_SERVICE_FACTORY_H_
