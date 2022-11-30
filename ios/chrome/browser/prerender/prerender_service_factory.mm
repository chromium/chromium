// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/prerender_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/prerender/prerender_service_impl.h"
#import "ios/chrome/browser/signin/account_consistency_service_factory.h"
#import "ios/web/public/browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

std::unique_ptr<KeyedService> BuildPrerenderService(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<PrerenderServiceImpl>(browser_state);
}

// static
PrerenderService* PrerenderServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<PrerenderService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
PrerenderServiceFactory* PrerenderServiceFactory::GetInstance() {
  static base::NoDestructor<PrerenderServiceFactory> instance;
  return instance.get();
}

PrerenderServiceFactory::PrerenderServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PrerenderService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::AccountConsistencyServiceFactory::GetInstance());
}

PrerenderServiceFactory::~PrerenderServiceFactory() {}

std::unique_ptr<KeyedService> PrerenderServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildPrerenderService(context);
}

// static
PrerenderServiceFactory::TestingFactory
PrerenderServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildPrerenderService);
}

bool PrerenderServiceFactory::ServiceIsNULLWhileTesting() const {
  // PrerenderService is omitted while testing because it complicates
  // measurements in perf tests.
  return true;
}
