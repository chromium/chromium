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
