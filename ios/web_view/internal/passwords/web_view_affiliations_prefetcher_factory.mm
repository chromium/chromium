
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/web_view_affiliations_prefetcher_factory.h"
#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/password_manager/core/browser/affiliation/affiliations_prefetcher.h"
#import "ios/web/public/browser_state.h"
#import "ios/web_view/internal/passwords/web_view_affiliation_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebViewAffiliationsPrefetcherFactory*
WebViewAffiliationsPrefetcherFactory::GetInstance() {
  static base::NoDestructor<WebViewAffiliationsPrefetcherFactory> instance;
  return instance.get();
}

password_manager::AffiliationsPrefetcher*
WebViewAffiliationsPrefetcherFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<password_manager::AffiliationsPrefetcher*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

WebViewAffiliationsPrefetcherFactory::WebViewAffiliationsPrefetcherFactory()
    : BrowserStateKeyedServiceFactory(
          "AffiliationsPrefetcher",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios_web_view::WebViewAffiliationServiceFactory::GetInstance());
}

WebViewAffiliationsPrefetcherFactory::~WebViewAffiliationsPrefetcherFactory() =
    default;

std::unique_ptr<KeyedService>
WebViewAffiliationsPrefetcherFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  password_manager::AffiliationService* affiliation_service =
      ios_web_view::WebViewAffiliationServiceFactory::GetForBrowserState(
          browser_state);
  return std::make_unique<password_manager::AffiliationsPrefetcher>(
      affiliation_service);
}
