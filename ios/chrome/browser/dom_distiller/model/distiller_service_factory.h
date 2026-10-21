// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DISTILLER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DISTILLER_SERVICE_FACTORY_H_

#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that creates the Distiller service for a given profile.
class DistillerServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<DistillerServiceFactory,
                                                DistillerService> {
 public:
  explicit DistillerServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_DOM_DISTILLER_MODEL_DISTILLER_SERVICE_FACTORY_H_
