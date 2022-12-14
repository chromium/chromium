// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_AFFILIATIONS_PREFETCHER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_AFFILIATIONS_PREFETCHER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace password_manager {
class AffiliationsPrefetcher;
}  // namespace password_manager

// Creates instances of AffiliationsPrefetcher per BrowserState.
class IOSChromeAffiliationsPrefetcherFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static IOSChromeAffiliationsPrefetcherFactory* GetInstance();
  static password_manager::AffiliationsPrefetcher* GetForBrowserState(
      web::BrowserState* browser_state);

 private:
  friend class base::NoDestructor<IOSChromeAffiliationsPrefetcherFactory>;

  IOSChromeAffiliationsPrefetcherFactory();
  ~IOSChromeAffiliationsPrefetcherFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_AFFILIATIONS_PREFETCHER_FACTORY_H_
