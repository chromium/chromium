// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRUSTED_VAULT_MODEL_IOS_TRUSTED_VAULT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_TRUSTED_VAULT_MODEL_IOS_TRUSTED_VAULT_SERVICE_FACTORY_H_

#import <memory>

#import "components/trusted_vault/trusted_vault_service.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Owns all TrustedVaultService instances and associates them to profiles.
class IOSTrustedVaultServiceFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          IOSTrustedVaultServiceFactory,
          trusted_vault::TrustedVaultService> {
 public:
  explicit IOSTrustedVaultServiceFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_TRUSTED_VAULT_MODEL_IOS_TRUSTED_VAULT_SERVICE_FACTORY_H_
