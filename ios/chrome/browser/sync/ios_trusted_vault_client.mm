// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/ios_trusted_vault_client.h"

#import "base/feature_list.h"
#import "base/functional/callback_helpers.h"
#import "base/task/sequenced_task_runner.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/trusted_vault/features.h"
#import "components/trusted_vault/trusted_vault_registration_verifier.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/trusted_vault_client_backend.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

constexpr base::TimeDelta kVerifyDeviceRegistrationDelay = base::Seconds(10);

}  // namespace

IOSTrustedVaultClient::IOSTrustedVaultClient(
    ChromeAccountManagerService* account_manager_service,
    signin::IdentityManager* identity_manager,
    TrustedVaultClientBackend* trusted_vault_client_backend,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : account_manager_service_(account_manager_service),
      backend_(trusted_vault_client_backend),
      registration_verifier_(identity_manager,
                             std::move(shared_url_loader_factory)) {
  DCHECK(account_manager_service_);
  DCHECK(backend_);

  if (base::FeatureList::IsEnabled(
          trusted_vault::kSyncTrustedVaultVerifyDeviceRegistration)) {
    backend_->SetDeviceRegistrationPublicKeyVerifierForUMA(
        base::BindOnce(&IOSTrustedVaultClient::VerifyDeviceRegistration,
                       weak_ptr_factory_.GetWeakPtr()));
  }
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

void IOSTrustedVaultClient::ClearLocalDataForAccount(
    const CoreAccountInfo& account_info) {
  backend_->ClearLocalData(IdentityForAccount(account_info), base::DoNothing());
}

id<SystemIdentity> IOSTrustedVaultClient::IdentityForAccount(
    const CoreAccountInfo& account_info) {
  return account_manager_service_->GetIdentityWithGaiaID(account_info.gaia);
}

void IOSTrustedVaultClient::VerifyDeviceRegistration(
    const std::string& gaia_id) {
  // It is possible for this method to be called with a `gaia_id` for an
  // account that is no longer known by the AccountManagerService. It is
  // not possible to verify the registration in that case, so bail out.
  //
  // One possible scenario is when an EG test signin with a real identity
  // and leak the gaia id in the backend, then another EG test restarts
  // the tested application with a fake identity service. In that case the
  // backend will call the registration with the previously recorded gaia
  // id but it will not be know to the identity service which will return
  // a null identity.
  //
  // See https://crbug.com/1448766 for investigation of the resulting crash.
  id<SystemIdentity> identity =
      account_manager_service_->GetIdentityWithGaiaID(gaia_id);
  if (!identity) {
    return;
  }

  backend_->GetPublicKeyForIdentity(
      identity,
      base::BindOnce(
          &IOSTrustedVaultClient::VerifyDeviceRegistrationWithPublicKey,
          weak_ptr_factory_.GetWeakPtr(), gaia_id));
}

void IOSTrustedVaultClient::VerifyDeviceRegistrationWithPublicKey(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key) {
  // Delay the logic, to be consistent with how other implementations do it.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &IOSTrustedVaultClient::VerifyDeviceRegistrationWithPublicKeyDelayed,
          weak_ptr_factory_.GetWeakPtr(), gaia_id, public_key),
      kVerifyDeviceRegistrationDelay);
}

void IOSTrustedVaultClient::VerifyDeviceRegistrationWithPublicKeyDelayed(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key) {
  // Note that this code is reachable once at most, because
  // SetDeviceRegistrationPublicKeyVerifierForUMA() registers a OnceCallback.
  registration_verifier_.VerifyMembership(gaia_id, public_key);
}
