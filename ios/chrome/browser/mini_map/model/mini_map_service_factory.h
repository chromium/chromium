// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MINI_MAP_MODEL_MINI_MAP_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_MINI_MAP_MODEL_MINI_MAP_SERVICE_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/mini_map/model/mini_map_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Factory for the MiniMapService that will observe the profile scoped settings.
class MiniMapServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<MiniMapServiceFactory,
                                                MiniMapService> {
 public:
  explicit MiniMapServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_MINI_MAP_MODEL_MINI_MAP_SERVICE_FACTORY_H_
