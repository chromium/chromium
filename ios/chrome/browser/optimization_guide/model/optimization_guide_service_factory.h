// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class OptimizationGuideService;

// Singleton that owns all OptimizationGuideService objects and associates them
// with Profiles.
class OptimizationGuideServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static OptimizationGuideService* GetForProfile(ProfileIOS* profile);
  static OptimizationGuideServiceFactory* GetInstance();

  // Initializes the prediction model store.
  static void InitializePredictionModelStore();

  // Returns the default factory used to build OptimizationGuideService. Can be
  // registered with SetTestingFactory to use real instances during testing.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<OptimizationGuideServiceFactory>;

  OptimizationGuideServiceFactory();
  ~OptimizationGuideServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_SERVICE_FACTORY_H_
