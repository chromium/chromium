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

TrustedVaultClientBackend::CancelDialogCallback
TrustedVaultClientBackend::Reauthentication(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  // The only caller is the overloaded `Reauthentication` method below.
  // The internal implementation either overloads this method or the method
  // below, which would then no longer call this method. Thus, this code can
  // never be reached.
  NOTREACHED();
}

TrustedVaultClientBackend::CancelDialogCallback
TrustedVaultClientBackend::Reauthentication(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    trusted_vault::TrustedVaultUserActionTriggerForUMA trigger,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  return Reauthentication(identity, security_domain_id,
                          presenting_view_controller, completion);
}

void TrustedVaultClientBackend::NotifyKeysChanged(
    trusted_vault::SecurityDomainId security_domain_id) {
  NotifyKeysChangedWithTrigger(security_domain_id, std::nullopt);
}

void TrustedVaultClientBackend::NotifyKeysChangedWithTrigger(
    trusted_vault::SecurityDomainId security_domain_id,
    std::optional<trusted_vault::TrustedVaultUserActionTriggerForUMA> trigger) {
  auto it = observer_lists_per_security_domain_id_.find(security_domain_id);
  if (it == observer_lists_per_security_domain_id_.end()) {
    return;
  }
  for (Observer& observer : it->second) {
    observer.OnTrustedVaultKeysChanged(trigger);
  }
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
