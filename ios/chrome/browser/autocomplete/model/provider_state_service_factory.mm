// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/provider_state_service_factory.h"

#import <utility>

#import "components/omnibox/browser/provider_state_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

ProviderStateServiceFactory::ProviderStateServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "ProviderStateService") {}

std::unique_ptr<KeyedService>
ProviderStateServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ProviderStateService>();
}

}  // namespace ios
