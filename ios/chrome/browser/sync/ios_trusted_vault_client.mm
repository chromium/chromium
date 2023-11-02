// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/ios_trusted_vault_client.h"

#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/trusted_vault_client_backend.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSTrustedVaultClient::IOSTrustedVaultClient(
    ChromeAccountManagerService* account_manager_service,
    TrustedVaultClientBackend* trusted_vault_client_backend)
    : account_manager_service_(account_manager_service),
      backend_(trusted_vault_client_backend) {
  DCHECK(account_manager_service_);
  DCHECK(backend_);
}

IOSTrustedVaultClient::~IOSTrustedVaultClient() = default;

void IOSTrustedVaultClient::AddObserver(Observer* observer) {
  backend_->AddObserver(observer);
}

void IOSTrustedVaultClient::RemoveObserver(Observer* observer) {
  backend_->RemoveObserver(observer);
}

void IOSTrustedVaultClient::FetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)>
        callback) {
  backend_->FetchKeys(IdentityForAccount(account_info), std::move(callback));
}

void IOSTrustedVaultClient::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  // Not used on iOS.
  NOTREACHED();
}

void IOSTrustedVaultClient::MarkLocalKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> callback) {
  backend_->MarkLocalKeysAsStale(
      IdentityForAccount(account_info),
      // Since false positives are allowed in the API, always invoke `callback`
      // with true, indicating that something may have changed.
      base::BindOnce(std::move(callback), true));
}

void IOSTrustedVaultClient::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> callback) {
  backend_->GetDegradedRecoverabilityStatus(IdentityForAccount(account_info),
                                            std::move(callback));
}

void IOSTrustedVaultClient::AddTrustedRecoveryMethod(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceClosure callback) {
  // Not used on iOS.
  NOTREACHED();
}

void IOSTrustedVaultClient::ClearDataForAccount(
    const CoreAccountInfo& account_info) {
  // TODO(crbug.com/1273080): decide whether this logic needs to be implemented
  // on iOS.
}

id<SystemIdentity> IOSTrustedVaultClient::IdentityForAccount(
    const CoreAccountInfo& account_info) {
  return account_manager_service_->GetIdentityWithGaiaID(account_info.gaia);
}
