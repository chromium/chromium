// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"

#import "base/no_destructor.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace feature_engagement {
namespace {

// static
std::unique_ptr<KeyedService> BuildServiceInstance(ProfileIOS* profile) {
  return CreateFeatureEngagementTracker(profile);
}

}  // namespace

// static
feature_engagement::Tracker* TrackerFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<feature_engagement::Tracker>(
      profile, /*create=*/true);
}

// static
TrackerFactory* TrackerFactory::GetInstance() {
  static base::NoDestructor<TrackerFactory> instance;
  return instance.get();
}

// Use the same tracker for regular and off-the-record Profiles.
TrackerFactory::TrackerFactory()
    : ProfileKeyedServiceFactoryIOS("feature_engagement::Tracker",
                                    ProfileSelection::kRedirectedInIncognito) {}

TrackerFactory::~TrackerFactory() = default;

// static
TrackerFactory::TestingFactory TrackerFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildServiceInstance);
}

std::unique_ptr<KeyedService> TrackerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return BuildServiceInstance(profile);
}

}  // namespace feature_engagement
