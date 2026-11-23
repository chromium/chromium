// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/trusted_vault/trusted_vault_client_backend.h"

#import <utility>

#import "base/containers/to_vector.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/notreached.h"
#import "components/trusted_vault/trusted_vault_key_and_version.h"

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

void TrustedVaultClientBackend::FetchKeys(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    KeysFetchedCallback completion) {
  NOTREACHED();
}

void TrustedVaultClientBackend::FetchKeys(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    KeysWithVersionsFetchedCallback completion) {
  FetchKeys(identity, security_domain_id,
            base::BindOnce([](const SharedKeyList& keys) {
              return base::ToVector(keys, [](const SharedKey& key) {
                // TODO(crbug.com/588274186): Remove this temporary fallback
                // (and the legacy `FetchKeys()` overload) once
                // `InternalTrustedVaultClientBackend` implements the versioned
                // `FetchKeys()` overload.
                return trusted_vault::TrustedVaultKeyAndVersion(key,
                                                                /*version=*/0);
              });
            }).Then(std::move(completion)));
}

void TrustedVaultClientBackend::NotifyKeysChanged(
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
