// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/trusted_vault/model/ios_trusted_vault_client.h"

#import "base/feature_list.h"
#import "base/functional/callback_helpers.h"
#import "base/task/sequenced_task_runner.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/trusted_vault/features.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/trusted_vault_client_backend.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

IOSTrustedVaultClient::IOSTrustedVaultClient(
    ChromeAccountManagerService* account_manager_service,
    signin::IdentityManager* identity_manager,
    TrustedVaultClientBackend* trusted_vault_client_backend,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : account_manager_service_(account_manager_service),
      backend_(trusted_vault_client_backend),
      security_domain_id_(trusted_vault::SecurityDomainId::kChromeSync) {
  DCHECK(account_manager_service_);
  DCHECK(backend_);
}

IOSTrustedVaultClient::~IOSTrustedVaultClient() = default;

void IOSTrustedVaultClient::AddObserver(Observer* observer) {
  backend_->AddObserver(observer, security_domain_id_);
}

void IOSTrustedVaultClient::RemoveObserver(Observer* observer) {
  backend_->RemoveObserver(observer, security_domain_id_);
}

void IOSTrustedVaultClient::FetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)>
        callback) {
  backend_->FetchKeys(IdentityForAccount(account_info), security_domain_id_,
                      std::move(callback));
}

void IOSTrustedVaultClient::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  // Not used on iOS.
  NOTREACHED_IN_MIGRATION();
}

void IOSTrustedVaultClient::MarkLocalKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> callback) {
  backend_->MarkLocalKeysAsStale(
      IdentityForAccount(account_info), security_domain_id_,
      // Since false positives are allowed in the API, always invoke `callback`
      // with true, indicating that something may have changed.
      base::BindOnce(std::move(callback), true));
}

void IOSTrustedVaultClient::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> callback) {
  backend_->GetDegradedRecoverabilityStatus(IdentityForAccount(account_info),
                                            security_domain_id_,
                                            std::move(callback));
}

void IOSTrustedVaultClient::AddTrustedRecoveryMethod(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceClosure callback) {
  // Not used on iOS.
  NOTREACHED_IN_MIGRATION();
}

void IOSTrustedVaultClient::ClearLocalDataForAccount(
    const CoreAccountInfo& account_info) {
  backend_->ClearLocalData(IdentityForAccount(account_info),
                           security_domain_id_, base::DoNothing());
}

id<SystemIdentity> IOSTrustedVaultClient::IdentityForAccount(
    const CoreAccountInfo& account_info) {
  return account_manager_service_->GetIdentityWithGaiaID(account_info.gaia);
}
