// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/trusted_vault/model/ios_trusted_vault_service_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "components/trusted_vault/trusted_vault_service.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/trusted_vault_client_backend_factory.h"
#import "ios/chrome/browser/trusted_vault/model/ios_trusted_vault_client.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
trusted_vault::TrustedVaultService*
IOSTrustedVaultServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<trusted_vault::TrustedVaultService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
IOSTrustedVaultServiceFactory* IOSTrustedVaultServiceFactory::GetInstance() {
  static base::NoDestructor<IOSTrustedVaultServiceFactory> instance;
  return instance.get();
}

IOSTrustedVaultServiceFactory::IOSTrustedVaultServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "TrustedVaultService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ChromeAccountManagerServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(TrustedVaultClientBackendFactory::GetInstance());
}

IOSTrustedVaultServiceFactory::~IOSTrustedVaultServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSTrustedVaultServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  CHECK(!browser_state->IsOffTheRecord());

  return std::make_unique<trusted_vault::TrustedVaultService>(
      /*chrome_sync_security_domain_client=*/
      std::make_unique<IOSTrustedVaultClient>(
          ChromeAccountManagerServiceFactory::GetForBrowserState(browser_state),
          IdentityManagerFactory::GetForBrowserState(browser_state),
          TrustedVaultClientBackendFactory::GetForBrowserState(browser_state),
          browser_state->GetSharedURLLoaderFactory()));
}
