// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_FACTORY_H_
#define IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace feature_engagement {
class Tracker;

// TrackerFactory is the main class for interacting with the
// feature_engagement component. It uses the KeyedService API to
// expose functions to associate and retrieve a feature_engagement::Tracker
// object with a given profile.
class TrackerFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static Tracker* GetForProfile(ProfileIOS* profile);
  static TrackerFactory* GetInstance();
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<TrackerFactory>;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  // Static helper for GetDefaultFactory.
  static std::unique_ptr<KeyedService> BuildServiceInstance(
      web::BrowserState* context);

  TrackerFactory();
  ~TrackerFactory() override;
};

}  // namespace feature_engagement

#endif  // IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_FACTORY_H_
