// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/trusted_vault/model/ios_trusted_vault_service_factory.h"

#import <utility>

#import "components/trusted_vault/trusted_vault_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/trusted_vault/trusted_vault_client_backend_factory.h"
#import "ios/chrome/browser/trusted_vault/model/ios_trusted_vault_client.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

IOSTrustedVaultServiceFactory::IOSTrustedVaultServiceFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "TrustedVaultService") {
  DependsOn(ChromeAccountManagerServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(TrustedVaultClientBackendFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSTrustedVaultServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  CHECK(!profile->IsOffTheRecord());

  return std::make_unique<trusted_vault::TrustedVaultService>(
      /*chrome_sync_security_domain_client=*/
      std::make_unique<IOSTrustedVaultClient>(
          ChromeAccountManagerServiceFactory::GetForProfile(profile),
          IdentityManagerFactory::GetForProfile(profile),
          TrustedVaultClientBackendFactory::GetForProfile(profile),
          profile->GetSharedURLLoaderFactory()),
      // TODO(crbug.com/540854648): Consider providing an IOSTrustedVaultClient
      // for SecurityDomainId::kPasskeys and migrating iOS passkey callers to
      // TrustedVaultService.
      /*passkeys_security_domain_client=*/nullptr);
}
