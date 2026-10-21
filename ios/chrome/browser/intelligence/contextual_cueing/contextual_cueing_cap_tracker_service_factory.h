// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CAP_TRACKER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CAP_TRACKER_SERVICE_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_cap_tracker_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace contextual_cueing {

// Singleton that owns all ContextualCueingCapTrackerServices and associates
// them with ProfileIOS.
class ContextualCueingCapTrackerServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          ContextualCueingCapTrackerServiceFactory,
          ContextualCueingCapTrackerService> {
 public:
  explicit ContextualCueingCapTrackerServiceFactory(PassKey key);
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace contextual_cueing

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_CAP_TRACKER_SERVICE_FACTORY_H_
