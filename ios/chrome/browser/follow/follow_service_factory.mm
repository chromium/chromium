// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/follow_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/follow/follow_configuration.h"
#import "ios/chrome/browser/follow/follow_service.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/public/provider/chrome/browser/follow/follow_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
FollowService* FollowServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<FollowService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
FollowServiceFactory* FollowServiceFactory::GetInstance() {
  static base::NoDestructor<FollowServiceFactory> instance;
  return instance.get();
}

FollowServiceFactory::FollowServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "FollowService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(DiscoverFeedServiceFactory::GetInstance());
}

FollowServiceFactory::~FollowServiceFactory() = default;

std::unique_ptr<KeyedService> FollowServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  FollowConfiguration* configuration = [[FollowConfiguration alloc] init];
  configuration.feedService =
      DiscoverFeedServiceFactory::GetForBrowserState(browser_state);

  return ios::provider::CreateFollowService(configuration);
}

void FollowServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kFirstFollowUIShownCount, 0);
}
