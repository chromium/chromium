// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/provider_state_service_factory.h"

#import "components/omnibox/browser/provider_state_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

// static
ProviderStateService* ProviderStateServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ProviderStateService>(
      profile, /*create=*/true);
}

// static
ProviderStateServiceFactory* ProviderStateServiceFactory::GetInstance() {
  static base::NoDestructor<ProviderStateServiceFactory> instance;
  return instance.get();
}

ProviderStateServiceFactory::ProviderStateServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ProviderStateService") {}

ProviderStateServiceFactory::~ProviderStateServiceFactory() = default;

std::unique_ptr<KeyedService>
ProviderStateServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ProviderStateService>();
}

}  // namespace ios
