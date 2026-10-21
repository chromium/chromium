// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ORIGIN_GATING_MODEL_ORIGIN_GATING_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_ORIGIN_GATING_MODEL_ORIGIN_GATING_SERVICE_FACTORY_H_

#import "components/origin_gating/core/origin_gating_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace origin_gating {

// Singleton that owns all OriginGatingServices and associates them with
// ProfileIOS.
class OriginGatingServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<OriginGatingServiceFactory,
                                                OriginGatingService> {
 public:
  explicit OriginGatingServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace origin_gating

#endif  // IOS_CHROME_BROWSER_ORIGIN_GATING_MODEL_ORIGIN_GATING_SERVICE_FACTORY_H_
