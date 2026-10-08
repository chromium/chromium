// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_SERVICE_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all OptimizationGuideService objects and associates them
// with Profiles.
class OptimizationGuideServiceFactory
    : public TypedProfileKeyedServiceFactoryIOS<OptimizationGuideServiceFactory,
                                                OptimizationGuideService> {
 public:
  OptimizationGuideServiceFactory(PassKey key);

  // Initializes the prediction model store.
  static void InitializePredictionModelStore();

  // Returns the default factory used to build OptimizationGuideService. Can be
  // registered with AddTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  // ProfileKeyedServiceFactoryIOS:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_SERVICE_FACTORY_H_
