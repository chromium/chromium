// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"

#import "base/no_destructor.h"
#import "base/time/default_clock.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_impl.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
PromosManager* PromosManagerFactory::GetForBrowserState(ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
PromosManager* PromosManagerFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<PromosManager*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
PromosManagerFactory* PromosManagerFactory::GetInstance() {
  static base::NoDestructor<PromosManagerFactory> instance;
  return instance.get();
}

PromosManagerFactory::PromosManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "PromosManagerFactory",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(feature_engagement::TrackerFactory::GetInstance());
}

PromosManagerFactory::~PromosManagerFactory() = default;

std::unique_ptr<KeyedService> PromosManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  auto promos_manager = std::make_unique<PromosManagerImpl>(
      GetApplicationContext()->GetLocalState(),
      base::DefaultClock::GetInstance(),
      feature_engagement::TrackerFactory::GetForProfile(profile));
  promos_manager->Init();
  return promos_manager;
}

web::BrowserState* PromosManagerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
