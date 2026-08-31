// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CAP_TRACKER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CAP_TRACKER_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace contextual_cueing {

class ContextualCueingCapTrackerService;

// Singleton that owns all ContextualCueingCapTrackerServices and associates
// them with ProfileIOS.
class ContextualCueingCapTrackerServiceFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static ContextualCueingCapTrackerService* GetForProfile(ProfileIOS* profile);
  static ContextualCueingCapTrackerServiceFactory* GetInstance();
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<ContextualCueingCapTrackerServiceFactory>;

  ContextualCueingCapTrackerServiceFactory();
  ~ContextualCueingCapTrackerServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace contextual_cueing

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CAP_TRACKER_SERVICE_FACTORY_H_
