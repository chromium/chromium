// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/origin_gating/model/origin_gating_service_factory.h"

#import <utility>

#import "base/types/pass_key.h"
#import "components/origin_gating/core/origin_gating_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace origin_gating {

OriginGatingServiceFactory::OriginGatingServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "OriginGatingService",
          ProfileSelection::kOwnInstanceInIncognito) {}

std::unique_ptr<KeyedService>
OriginGatingServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return std::make_unique<OriginGatingService>(
      base::PassKey<OriginGatingServiceFactory>());
}

}  // namespace origin_gating
