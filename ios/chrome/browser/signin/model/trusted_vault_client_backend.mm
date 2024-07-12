// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/trusted_vault_client_backend.h"

#import "base/functional/callback.h"

TrustedVaultClientBackend::TrustedVaultClientBackend() = default;

TrustedVaultClientBackend::~TrustedVaultClientBackend() = default;

void TrustedVaultClientBackend::FetchKeys(id<SystemIdentity> identity,
                                          KeyFetchedCallback callback) {
  FetchKeys(identity, nil, std::move(callback));
}

void TrustedVaultClientBackend::FetchKeys(id<SystemIdentity> identity,
                                          NSString* security_domain,
                                          KeyFetchedCallback completion) {
  NOTREACHED_NORETURN();
}

void TrustedVaultClientBackend::MarkLocalKeysAsStale(
    id<SystemIdentity> identity,
    base::OnceClosure callback) {
  MarkLocalKeysAsStale(identity, nil, std::move(callback));
}

void TrustedVaultClientBackend::MarkLocalKeysAsStale(
    id<SystemIdentity> identity,
    NSString* security_domain,
    base::OnceClosure completion) {
  NOTREACHED_NORETURN();
}

void TrustedVaultClientBackend::GetDegradedRecoverabilityStatus(
    id<SystemIdentity> identity,
    base::OnceCallback<void(bool)> callback) {
  GetDegradedRecoverabilityStatus(identity, nil, std::move(callback));
}

void TrustedVaultClientBackend::GetDegradedRecoverabilityStatus(
    id<SystemIdentity> identity,
    NSString* security_domain,
    base::OnceCallback<void(bool)> completion) {
  NOTREACHED_NORETURN();
}

void TrustedVaultClientBackend::Reauthentication(
    id<SystemIdentity> identity,
    UIViewController* presenting_view_controller,
    CompletionBlock callback) {
  Reauthentication(identity, nil, presenting_view_controller,
                   std::move(callback));
}

void TrustedVaultClientBackend::Reauthentication(
    id<SystemIdentity> identity,
    NSString* security_domain,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  NOTREACHED_NORETURN();
}

void TrustedVaultClientBackend::FixDegradedRecoverability(
    id<SystemIdentity> identity,
    UIViewController* presenting_view_controller,
    CompletionBlock callback) {
  FixDegradedRecoverability(identity, nil, presenting_view_controller,
                            std::move(callback));
}
void TrustedVaultClientBackend::FixDegradedRecoverability(
    id<SystemIdentity> identity,
    NSString* security_domain,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  NOTREACHED_NORETURN();
}

void TrustedVaultClientBackend::ClearLocalData(
    id<SystemIdentity> identity,
    base::OnceCallback<void(bool)> callback) {
  ClearLocalData(identity, nil, std::move(callback));
}

void TrustedVaultClientBackend::ClearLocalData(
    id<SystemIdentity> identity,
    NSString* security_domain,
    base::OnceCallback<void(bool)> completion) {
  NOTREACHED_NORETURN();
}
