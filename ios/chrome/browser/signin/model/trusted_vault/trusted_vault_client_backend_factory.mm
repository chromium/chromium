// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/trusted_vault/trusted_vault_client_backend_factory.h"

#import <utility>

#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/trusted_vault/trusted_vault_client_backend.h"
#import "ios/chrome/browser/signin/model/trusted_vault/trusted_vault_configuration.h"
#import "ios/public/provider/chrome/browser/signin/trusted_vault_api.h"

TrustedVaultClientBackendFactory::TrustedVaultClientBackendFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "TrustedVaultClientBackend") {}

std::unique_ptr<KeyedService>
TrustedVaultClientBackendFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  // Give the opportunity for the test hook to override the factory from
  // the provider (allowing EG tests to use a fake TrustedVaultClientBackend).
  if (auto backend = tests_hook::CreateTrustedVaultClientBackend()) {
    return backend;
  }

  TrustedVaultConfiguration* configuration =
      [[TrustedVaultConfiguration alloc] init];
  ApplicationContext* application_context = GetApplicationContext();
  configuration.singleSignOnService =
      application_context->GetSingleSignOnService();
  return ios::provider::CreateTrustedVaultClientBackend(configuration);
}
