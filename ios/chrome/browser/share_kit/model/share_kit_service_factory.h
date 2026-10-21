// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SERVICE_FACTORY_H_

#import <memory>

#include "ios/chrome/browser/share_kit/model/share_kit_service.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all instances of ShareKitService and associates them with
// instances of ProfileIOS.
class ShareKitServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<ShareKitServiceFactory,
                                                ShareKitService> {
 public:
  explicit ShareKitServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SERVICE_FACTORY_H_
