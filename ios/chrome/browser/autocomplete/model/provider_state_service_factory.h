// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_PROVIDER_STATE_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_PROVIDER_STATE_SERVICE_FACTORY_H_

#import "components/omnibox/browser/provider_state_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace ios {

// Singleton that owns all ProviderStateServices and associates them with
// profiles.
class ProviderStateServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<ProviderStateServiceFactory,
                                                ProviderStateService> {
 public:
  explicit ProviderStateServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_PROVIDER_STATE_SERVICE_FACTORY_H_
