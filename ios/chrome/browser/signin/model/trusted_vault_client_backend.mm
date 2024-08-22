// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/trusted_vault_client_backend.h"

#import "base/functional/callback.h"

TrustedVaultClientBackend::TrustedVaultClientBackend() = default;

TrustedVaultClientBackend::~TrustedVaultClientBackend() = default;

void TrustedVaultClientBackend::AddObserver(
    TrustedVaultClientBackend::Observer* observer,
    const std::string& security_domain_path,
    trusted_vault::SecurityDomainId security_domain_id) {
  observer_lists_per_security_domain_path_[security_domain_path].AddObserver(
      observer);
}

void TrustedVaultClientBackend::RemoveObserver(
    Observer* observer,
    const std::string& security_domain_path,
    trusted_vault::SecurityDomainId security_domain_id) {
  observer_lists_per_security_domain_path_[security_domain_path].RemoveObserver(
      observer);
}

void TrustedVaultClientBackend::NotifyKeysChangedWithTwoSecurityDomainParameter(
    const std::string& security_domain_path,
    trusted_vault::SecurityDomainId security_domain_id) {
  NotifyKeysChanged(security_domain_path);
}

void TrustedVaultClientBackend::NotifyKeysChanged(
    const std::string& security_domain_path) {
  auto it = observer_lists_per_security_domain_path_.find(security_domain_path);
  if (it == observer_lists_per_security_domain_path_.end()) {
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
  TrustedVaultClientBackend::NotifyRecoverabilityChanged(security_domain_path);
}
void TrustedVaultClientBackend::NotifyRecoverabilityChanged(
    const std::string& security_domain_path) {
  auto it = observer_lists_per_security_domain_path_.find(security_domain_path);
  if (it == observer_lists_per_security_domain_path_.end()) {
    return;
  }
  for (Observer& observer : it->second) {
    observer.OnTrustedVaultRecoverabilityChanged();
  }
}

#pragma mark - Deprecated methods, use for migration only.

void TrustedVaultClientBackend::FetchKeys(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    KeyFetchedCallback completion) {
  NOTREACHED();
}

void TrustedVaultClientBackend::MarkLocalKeysAsStale(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    base::OnceClosure completion) {
  NOTREACHED();
}

void TrustedVaultClientBackend::GetDegradedRecoverabilityStatus(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    base::OnceCallback<void(bool)> completion) {
  NOTREACHED();
}

TrustedVaultClientBackend::CancelDialogCallback
TrustedVaultClientBackend::Reauthentication(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  NOTREACHED();
}

TrustedVaultClientBackend::CancelDialogCallback
TrustedVaultClientBackend::FixDegradedRecoverability(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  NOTREACHED();
}
void TrustedVaultClientBackend::ClearLocalData(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    base::OnceCallback<void(bool)> completion) {
  NOTREACHED();
}

#pragma mark - Virtual functions
// Those functions call the deprecated function. They will be purely virtual
// after the migraiton is done.
void TrustedVaultClientBackend::FetchKeys(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    trusted_vault::SecurityDomainId security_domain_id,
    KeyFetchedCallback completion) {
  FetchKeys(identity, security_domain_path, std::move(completion));
}
void TrustedVaultClientBackend::MarkLocalKeysAsStale(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    trusted_vault::SecurityDomainId security_domain_id,
    base::OnceClosure completion) {
  MarkLocalKeysAsStale(identity, security_domain_path, std::move(completion));
}
void TrustedVaultClientBackend::GetDegradedRecoverabilityStatus(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    trusted_vault::SecurityDomainId security_domain_id,
    base::OnceCallback<void(bool)> completion) {
  GetDegradedRecoverabilityStatus(identity, security_domain_path,
                                  std::move(completion));
}
TrustedVaultClientBackend::CancelDialogCallback
TrustedVaultClientBackend::Reauthentication(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    trusted_vault::SecurityDomainId security_domain_id,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  return Reauthentication(identity, security_domain_path,
                          presenting_view_controller, std::move(completion));
}
TrustedVaultClientBackend::CancelDialogCallback
TrustedVaultClientBackend::FixDegradedRecoverability(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    trusted_vault::SecurityDomainId security_domain_id,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  return FixDegradedRecoverability(identity, security_domain_path,
                                   presenting_view_controller,
                                   std::move(completion));
}
void TrustedVaultClientBackend::ClearLocalData(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    trusted_vault::SecurityDomainId security_domain_id,
    base::OnceCallback<void(bool)> completion) {
  return ClearLocalData(identity, security_domain_path, std::move(completion));
}
