// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/ios_trusted_vault_client.h"

#include "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_trusted_vault_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSTrustedVaultClient::IOSTrustedVaultClient(
    ChromeAccountManagerService* account_manager_service)
    : account_manager_service_(account_manager_service) {
  DCHECK(account_manager_service_);
}

IOSTrustedVaultClient::~IOSTrustedVaultClient() = default;

void IOSTrustedVaultClient::AddObserver(Observer* observer) {
  ios::ChromeBrowserProvider* browser_provider =
      ios::GetChromeBrowserProvider();
  ios::ChromeTrustedVaultService* trusted_vault_service =
      browser_provider->GetChromeTrustedVaultService();
  if (trusted_vault_service) {
    trusted_vault_service->AddObserver(observer);
  }
}

void IOSTrustedVaultClient::RemoveObserver(Observer* observer) {
  ios::ChromeBrowserProvider* browser_provider =
      ios::GetChromeBrowserProvider();
  ios::ChromeTrustedVaultService* trusted_vault_service =
      browser_provider->GetChromeTrustedVaultService();
  if (trusted_vault_service) {
    trusted_vault_service->RemoveObserver(observer);
  }
}

void IOSTrustedVaultClient::FetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)>
        callback) {
  ChromeIdentity* identity =
      account_manager_service_->GetIdentityWithGaiaID(account_info.gaia);

  ios::ChromeBrowserProvider* browser_provider =
      ios::GetChromeBrowserProvider();
  ios::ChromeTrustedVaultService* trusted_vault_service =
      browser_provider->GetChromeTrustedVaultService();
  DCHECK(trusted_vault_service);
  trusted_vault_service->FetchKeys(identity, std::move(callback));
}

void IOSTrustedVaultClient::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  // Not used on iOS.
  NOTREACHED();
}

void IOSTrustedVaultClient::MarkKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/1100278): Needs implementation.
  std::move(callback).Run(false);
}

void IOSTrustedVaultClient::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> callback) {
  ChromeIdentity* identity =
      account_manager_service_->GetIdentityWithGaiaID(account_info.gaia);

  ios::GetChromeBrowserProvider()
      ->GetChromeTrustedVaultService()
      ->GetDegradedRecoverabilityStatus(identity, std::move(callback));
}

void IOSTrustedVaultClient::AddTrustedRecoveryMethod(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceClosure callback) {
  // TODO(crbug.com/1100278): Needs implementation.
  std::move(callback).Run();
}
