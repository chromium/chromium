// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"

#import <utility>

#import "base/time/default_clock.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

PromosManagerFactory::PromosManagerFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "PromosManagerFactory",
          ProfileSelection::kRedirectedInIncognito) {
  DependsOn(feature_engagement::TrackerFactory::GetInstance());
}

std::unique_ptr<KeyedService> PromosManagerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  auto promos_manager = std::make_unique<PromosManagerImpl>(
      profile->GetPrefs(), base::DefaultClock::GetInstance(),
      feature_engagement::TrackerFactory::GetForProfile(profile));
  promos_manager->Init();
  return promos_manager;
}
