// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"

#import <utility>

#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace feature_engagement {

// Use the same tracker for regular and off-the-record Profiles.
TrackerFactory::TrackerFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "feature_engagement::Tracker",
          ProfileSelection::kRedirectedInIncognito) {}

// static
TrackerFactory::TestingFactoryByRef TrackerFactory::GetDefaultFactory() {
  return base::BindOnce(&CreateFeatureEngagementTracker);
}

std::unique_ptr<KeyedService> TrackerFactory::BuildServiceInstanceFor(
    ProfileIOS& profile) const {
  return CreateFeatureEngagementTracker(profile);
}

}  // namespace feature_engagement
