// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/model/ios_chrome_affiliations_prefetcher_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/password_manager/core/browser/affiliation/affiliations_prefetcher.h"
#include "ios/chrome/browser/passwords/model/ios_chrome_affiliation_service_factory.h"
#include "ios/web/public/browser_state.h"

IOSChromeAffiliationsPrefetcherFactory*
IOSChromeAffiliationsPrefetcherFactory::GetInstance() {
  static base::NoDestructor<IOSChromeAffiliationsPrefetcherFactory> instance;
  return instance.get();
}

password_manager::AffiliationsPrefetcher*
IOSChromeAffiliationsPrefetcherFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<password_manager::AffiliationsPrefetcher*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

IOSChromeAffiliationsPrefetcherFactory::IOSChromeAffiliationsPrefetcherFactory()
    : BrowserStateKeyedServiceFactory(
          "AffiliationsPrefetcher",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IOSChromeAffiliationServiceFactory::GetInstance());
}

IOSChromeAffiliationsPrefetcherFactory::
    ~IOSChromeAffiliationsPrefetcherFactory() = default;

std::unique_ptr<KeyedService>
IOSChromeAffiliationsPrefetcherFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  password_manager::AffiliationService* affiliation_service =
      IOSChromeAffiliationServiceFactory::GetForBrowserState(browser_state);
  return std::make_unique<password_manager::AffiliationsPrefetcher>(
      affiliation_service);
}
