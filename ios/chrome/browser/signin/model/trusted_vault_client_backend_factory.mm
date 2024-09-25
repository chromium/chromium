// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/trusted_vault_client_backend_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/trusted_vault_client_backend.h"
#import "ios/chrome/browser/signin/model/trusted_vault_configuration.h"
#import "ios/public/provider/chrome/browser/signin/trusted_vault_api.h"

// static
TrustedVaultClientBackend* TrustedVaultClientBackendFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<TrustedVaultClientBackend*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
TrustedVaultClientBackendFactory*
TrustedVaultClientBackendFactory::GetInstance() {
  static base::NoDestructor<TrustedVaultClientBackendFactory> instance;
  return instance.get();
}

TrustedVaultClientBackendFactory::TrustedVaultClientBackendFactory()
    : BrowserStateKeyedServiceFactory(
          "TrustedVaultClientBackend",
          BrowserStateDependencyManager::GetInstance()) {}

TrustedVaultClientBackendFactory::~TrustedVaultClientBackendFactory() = default;

std::unique_ptr<KeyedService>
TrustedVaultClientBackendFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
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
