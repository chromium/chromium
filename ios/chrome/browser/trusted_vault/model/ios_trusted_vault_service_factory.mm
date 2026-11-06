// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/trusted_vault/model/ios_trusted_vault_service_factory.h"

#import <utility>

#import "components/trusted_vault/trusted_vault_server_constants.h"
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
    ProfileIOS& profile) const {
  CHECK(!profile.IsOffTheRecord());

  ChromeAccountManagerService* account_manager_service =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  TrustedVaultClientBackend* backend =
      TrustedVaultClientBackendFactory::GetForProfile(profile);
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
      profile.GetSharedURLLoaderFactory();

  return std::make_unique<trusted_vault::TrustedVaultService>(
      /*chrome_sync_security_domain_client=*/
      std::make_unique<IOSTrustedVaultClient>(
          account_manager_service, identity_manager, backend,
          shared_url_loader_factory,
          trusted_vault::SecurityDomainId::kChromeSync),
      /*passkeys_security_domain_client=*/
      std::make_unique<IOSTrustedVaultClient>(
          account_manager_service, identity_manager, backend,
          shared_url_loader_factory,
          trusted_vault::SecurityDomainId::kPasskeys));
}
