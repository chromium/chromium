// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/trusted_vault_client_backend.h"

#import "base/functional/callback.h"

TrustedVaultClientBackend::TrustedVaultClientBackend() = default;

TrustedVaultClientBackend::~TrustedVaultClientBackend() = default;

void TrustedVaultClientBackend::AddObserver(
    TrustedVaultClientBackend::Observer* observer,
    trusted_vault::SecurityDomainId security_domain_id) {
  observer_lists_per_security_domain_id_[security_domain_id].AddObserver(
      observer);
}

void TrustedVaultClientBackend::RemoveObserver(
    Observer* observer,
    trusted_vault::SecurityDomainId security_domain_id) {
  observer_lists_per_security_domain_id_[security_domain_id].RemoveObserver(
      observer);
}

void TrustedVaultClientBackend::NotifyKeysChangedWithTwoSecurityDomainParameter(
    const std::string& security_domain_path,
    trusted_vault::SecurityDomainId security_domain_id) {
  NotifyKeysChanged(security_domain_id);
}

void TrustedVaultClientBackend::NotifyKeysChanged(
    trusted_vault::SecurityDomainId security_domain_id) {
  auto it = observer_lists_per_security_domain_id_.find(security_domain_id);
  if (it == observer_lists_per_security_domain_id_.end()) {
    return;
  }
  for (Observer& observer : it->second) {
    observer.OnTrustedVaultKeysChanged();
  }
}

void TrustedVaultClientBackend::
    NotifyRecoverabilityChangedWithTwoSecurityDomainParameter(
        const std::string& security_domain_path,
        trusted_vault::SecurityDomainId security_domain_id) {
  TrustedVaultClientBackend::NotifyRecoverabilityChanged(security_domain_id);
}
void TrustedVaultClientBackend::NotifyRecoverabilityChanged(
    trusted_vault::SecurityDomainId security_domain_id) {
  auto it = observer_lists_per_security_domain_id_.find(security_domain_id);
  if (it == observer_lists_per_security_domain_id_.end()) {
    return;
  }
  for (Observer& observer : it->second) {
    observer.OnTrustedVaultRecoverabilityChanged();
  }
}

#pragma mark - Pure virtual method
// Those methods are implemented only for the time of migration.
void TrustedVaultClientBackend::FetchKeys(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    KeyFetchedCallback completion) {
  NOTREACHED();
}

void TrustedVaultClientBackend::MarkLocalKeysAsStale(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    base::OnceClosure completion) {
  NOTREACHED();
}

void TrustedVaultClientBackend::GetDegradedRecoverabilityStatus(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    base::OnceCallback<void(bool)> completion) {
  NOTREACHED();
}

TrustedVaultClientBackend::CancelDialogCallback
TrustedVaultClientBackend::Reauthentication(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  NOTREACHED();
}

TrustedVaultClientBackend::CancelDialogCallback
TrustedVaultClientBackend::FixDegradedRecoverability(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  NOTREACHED();
}
void TrustedVaultClientBackend::ClearLocalData(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    base::OnceCallback<void(bool)> completion) {
  NOTREACHED();
}

#pragma mark - Deprecated method. For migration only.
// Those functions calls the actual implementation. They will be deleted.
void TrustedVaultClientBackend::FetchKeys(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    trusted_vault::SecurityDomainId security_domain_id,
    KeyFetchedCallback completion) {
  FetchKeys(identity, security_domain_id, std::move(completion));
}
void TrustedVaultClientBackend::MarkLocalKeysAsStale(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    trusted_vault::SecurityDomainId security_domain_id,
    base::OnceClosure completion) {
  MarkLocalKeysAsStale(identity, security_domain_id, std::move(completion));
}
void TrustedVaultClientBackend::GetDegradedRecoverabilityStatus(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    trusted_vault::SecurityDomainId security_domain_id,
    base::OnceCallback<void(bool)> completion) {
  GetDegradedRecoverabilityStatus(identity, security_domain_id,
                                  std::move(completion));
}
TrustedVaultClientBackend::CancelDialogCallback
TrustedVaultClientBackend::Reauthentication(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    trusted_vault::SecurityDomainId security_domain_id,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  return Reauthentication(identity, security_domain_id,
                          presenting_view_controller, std::move(completion));
}
TrustedVaultClientBackend::CancelDialogCallback
TrustedVaultClientBackend::FixDegradedRecoverability(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    trusted_vault::SecurityDomainId security_domain_id,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  return FixDegradedRecoverability(identity, security_domain_id,
                                   presenting_view_controller,
                                   std::move(completion));
}
void TrustedVaultClientBackend::ClearLocalData(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    trusted_vault::SecurityDomainId security_domain_id,
    base::OnceCallback<void(bool)> completion) {
  return ClearLocalData(identity, security_domain_id, std::move(completion));
}
