// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ON_DEVICE_TAIL_MODEL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ON_DEVICE_TAIL_MODEL_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class KeyedService;
class ProfileIOS;
class OnDeviceTailModelService;

// A factory to create a unique `OnDeviceTailModelService` per
// profile. Has dependency on `OptimizationGuideKeyedServiceFactory`.
class OnDeviceTailModelServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Gets the singleton instance of `OnDeviceTailModelServiceFactory`.
  static OnDeviceTailModelServiceFactory* GetInstance();

  // Gets the `OnDeviceTailModelService` for the profile.
  static OnDeviceTailModelService* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<OnDeviceTailModelServiceFactory>;

  OnDeviceTailModelServiceFactory();
  ~OnDeviceTailModelServiceFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  //
  // Returns nullptr if `OptimizationGuideKeyedService` is null.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ON_DEVICE_TAIL_MODEL_SERVICE_FACTORY_H_
