// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_FACTORY_H_
#define IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace feature_engagement {
class Tracker;

// TrackerFactory is the main class for interacting with the
// feature_engagement component. It uses the KeyedService API to
// expose functions to associate and retrieve a feature_engagement::Tracker
// object with a given profile.
class TrackerFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static Tracker* GetForBrowserState(ProfileIOS* profile);

  static Tracker* GetForProfile(ProfileIOS* profile);
  static TrackerFactory* GetInstance();

  TrackerFactory(const TrackerFactory&) = delete;
  TrackerFactory& operator=(const TrackerFactory&) = delete;

 protected:
  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;

 private:
  friend class base::NoDestructor<TrackerFactory>;

  TrackerFactory();
  ~TrackerFactory() override;
};

}  // namespace feature_engagement

#endif  // IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_FACTORY_H_
