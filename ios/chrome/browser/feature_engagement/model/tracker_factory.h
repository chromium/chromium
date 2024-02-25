// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_FACTORY_H_
#define IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;

namespace feature_engagement {
class Tracker;

// TrackerFactory is the main class for interacting with the
// feature_engagement component. It uses the KeyedService API to
// expose functions to associate and retrieve a feature_engagement::Tracker
// object with a given ChromeBrowserState object.
class TrackerFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the TrackerFactory singleton object.
  static TrackerFactory* GetInstance();

  // Retrieves the Tracker object associated with a given
  // browser state. It is created if it does not already exist.
  static Tracker* GetForBrowserState(ChromeBrowserState* browser_state);

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
