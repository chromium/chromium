// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/trusted_vault_client_backend.h"

BASE_FEATURE(kTrustedVaultSecurityDomainKillSwitch,
             "TrustedVaultSecurityDomainKillSwitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

TrustedVaultClientBackend::TrustedVaultClientBackend() = default;

TrustedVaultClientBackend::~TrustedVaultClientBackend() = default;

void TrustedVaultClientBackend::AddObserver(
    TrustedVaultClientBackend::Observer* observer,
    const std::string& security_domain_path) {
  observer_lists_per_security_domain_path_[security_domain_path].AddObserver(
      observer);
}

void TrustedVaultClientBackend::RemoveObserver(
    Observer* observer,
    const std::string& security_domain_path) {
  observer_lists_per_security_domain_path_[security_domain_path].RemoveObserver(
      observer);
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
