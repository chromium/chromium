// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/trusted_vault/model/ios_trusted_vault_service_factory.h"

#import "base/no_destructor.h"
#import "components/trusted_vault/trusted_vault_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/trusted_vault_client_backend_factory.h"
#import "ios/chrome/browser/trusted_vault/model/ios_trusted_vault_client.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
trusted_vault::TrustedVaultService*
IOSTrustedVaultServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<trusted_vault::TrustedVaultService>(
          profile, /*create=*/true);
}

// static
IOSTrustedVaultServiceFactory* IOSTrustedVaultServiceFactory::GetInstance() {
  static base::NoDestructor<IOSTrustedVaultServiceFactory> instance;
  return instance.get();
}

IOSTrustedVaultServiceFactory::IOSTrustedVaultServiceFactory()
    : ProfileKeyedServiceFactoryIOS("TrustedVaultService") {
  DependsOn(ChromeAccountManagerServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(TrustedVaultClientBackendFactory::GetInstance());
}

IOSTrustedVaultServiceFactory::~IOSTrustedVaultServiceFactory() = default;

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
          profile->GetSharedURLLoaderFactory()));
}
