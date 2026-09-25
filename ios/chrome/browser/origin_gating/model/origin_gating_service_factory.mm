// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/origin_gating/model/origin_gating_service_factory.h"

#import "base/no_destructor.h"
#import "base/types/pass_key.h"
#import "components/origin_gating/core/origin_gating_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace origin_gating {

// static
OriginGatingService* OriginGatingServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<OriginGatingService>(
      profile, /*create=*/true);
}

// static
OriginGatingServiceFactory* OriginGatingServiceFactory::GetInstance() {
  static base::NoDestructor<OriginGatingServiceFactory> instance;
  return instance.get();
}

OriginGatingServiceFactory::OriginGatingServiceFactory()
    : ProfileKeyedServiceFactoryIOS("OriginGatingService",
                                    ProfileSelection::kOwnInstanceInIncognito) {
}

OriginGatingServiceFactory::~OriginGatingServiceFactory() = default;

std::unique_ptr<KeyedService>
OriginGatingServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return std::make_unique<OriginGatingService>(
      base::PassKey<OriginGatingServiceFactory>());
}

}  // namespace origin_gating
