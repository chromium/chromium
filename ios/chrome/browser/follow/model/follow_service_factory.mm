// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/model/follow_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/follow/model/follow_configuration.h"
#import "ios/chrome/browser/follow/model/follow_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/public/provider/chrome/browser/follow/follow_api.h"

// static
FollowService* FollowServiceFactory::GetForProfile(ProfileIOS* profile) {
  return static_cast<FollowService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
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
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  FollowConfiguration* configuration = [[FollowConfiguration alloc] init];
  configuration.feedService =
      DiscoverFeedServiceFactory::GetForProfile(profile);

  return ios::provider::CreateFollowService(configuration);
}

void FollowServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kFirstFollowUIShownCount, 0);
  registry->RegisterIntegerPref(prefs::kFirstFollowUpdateUIShownCount, 0);
}
