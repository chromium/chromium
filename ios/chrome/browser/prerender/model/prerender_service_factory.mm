// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/prerender/model/prerender_service_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/prerender/model/prerender_service_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/account_consistency_service_factory.h"

namespace {

// Default factory for PrerenderServiceFactory.
std::unique_ptr<KeyedService> BuildPrerenderService(
    web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<PrerenderServiceImpl>(profile);
}

}  // anonymous namespace

// static
PrerenderService* PrerenderServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<PrerenderService>(
      profile, /*create=*/true);
}

// static
PrerenderServiceFactory* PrerenderServiceFactory::GetInstance() {
  static base::NoDestructor<PrerenderServiceFactory> instance;
  return instance.get();
}

// PrerenderService is omitted while testing because it complicates
// measurements in perf tests.
PrerenderServiceFactory::PrerenderServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PrerenderService",
                                    TestingCreation::kNoServiceForTests) {
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
