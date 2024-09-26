// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/prerender/model/prerender_service_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/account_consistency_service_factory.h"
#import "ios/web/public/browser_state.h"

std::unique_ptr<KeyedService> BuildPrerenderService(
    web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<PrerenderServiceImpl>(profile);
}

// static
PrerenderService* PrerenderServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<PrerenderService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
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
