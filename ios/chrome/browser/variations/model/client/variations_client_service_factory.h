// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_VARIATIONS_MODEL_CLIENT_VARIATIONS_CLIENT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_VARIATIONS_MODEL_CLIENT_VARIATIONS_CLIENT_SERVICE_FACTORY_H_

#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"
#include "ios/chrome/browser/variations/model/client/variations_client_service.h"

// Factory for VariationsClientService.
class VariationsClientServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<VariationsClientServiceFactory,
                                                VariationsClientService> {
 public:
  explicit VariationsClientServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_VARIATIONS_MODEL_CLIENT_VARIATIONS_CLIENT_SERVICE_FACTORY_H_
