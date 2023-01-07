// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/safe_browsing_client_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#import "ios/chrome/browser/safe_browsing/real_time_url_lookup_service_factory.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_client_impl.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_client.h"
#import "ios/web/public/browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
SafeBrowsingClient* SafeBrowsingClientFactory::GetForBrowserState(
    web::BrowserState* browser_state) {
  return static_cast<SafeBrowsingClient*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
SafeBrowsingClientFactory* SafeBrowsingClientFactory::GetInstance() {
  static base::NoDestructor<SafeBrowsingClientFactory> instance;
  return instance.get();
}

SafeBrowsingClientFactory::SafeBrowsingClientFactory()
    : BrowserStateKeyedServiceFactory(
          "SafeBrowsingClient",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(RealTimeUrlLookupServiceFactory::GetInstance());
  DependsOn(PrerenderServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SafeBrowsingClientFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  safe_browsing::RealTimeUrlLookupService* lookup_service =
      RealTimeUrlLookupServiceFactory::GetForBrowserState(browser_state);
  PrerenderService* prerender_service =
      PrerenderServiceFactory::GetForBrowserState(browser_state);
  return std::make_unique<SafeBrowsingClientImpl>(lookup_service,
                                                  prerender_service);
}

web::BrowserState* SafeBrowsingClientFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
