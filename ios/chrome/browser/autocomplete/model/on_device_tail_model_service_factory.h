// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ON_DEVICE_TAIL_MODEL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ON_DEVICE_TAIL_MODEL_SERVICE_FACTORY_H_

#import <memory>

#include "components/omnibox/browser/on_device_tail_model_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// A factory to create a unique `OnDeviceTailModelService` per
// profile. Has dependency on `OptimizationGuideServiceFactory`.
class OnDeviceTailModelServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<OnDeviceTailModelServiceFactory,
                                                OnDeviceTailModelService> {
 public:
  explicit OnDeviceTailModelServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  //
  // Returns nullptr if `OptimizationGuideKeyedService` is null.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_ON_DEVICE_TAIL_MODEL_SERVICE_FACTORY_H_
