// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_FACTORY_H_
#define IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_FACTORY_H_

#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace feature_engagement {

// TrackerFactory is the main class for interacting with the
// feature_engagement component. It uses the KeyedService API to
// expose functions to associate and retrieve a feature_engagement::Tracker
// object with a given profile.
class TrackerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<TrackerFactory, Tracker> {
 public:
  explicit TrackerFactory(PassKey key);
  static TestingFactoryByRef GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS& profile) const final;
};

}  // namespace feature_engagement

#endif  // IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_FACTORY_H_
