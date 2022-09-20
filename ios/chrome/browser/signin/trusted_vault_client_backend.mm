// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/trusted_vault_client_backend.h"

#import "base/callback.h"
#import "base/notreached.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TrustedVaultClientBackend::TrustedVaultClientBackend() = default;

TrustedVaultClientBackend::~TrustedVaultClientBackend() = default;

void TrustedVaultClientBackend::FetchKeys(ChromeIdentity* chrome_identity,
                                          KeyFetchedCallback callback) {
  FetchKeys(static_cast<id<SystemIdentity>>(chrome_identity),
            std::move(callback));
}

void TrustedVaultClientBackend::FetchKeys(id<SystemIdentity> identity,
                                          KeyFetchedCallback callback) {
  NOTREACHED();
}

void TrustedVaultClientBackend::MarkLocalKeysAsStale(
    ChromeIdentity* chrome_identity,
    base::OnceClosure callback) {
  MarkLocalKeysAsStale(static_cast<id<SystemIdentity>>(chrome_identity),
                       std::move(callback));
}

void TrustedVaultClientBackend::MarkLocalKeysAsStale(
    id<SystemIdentity> identity,
    base::OnceClosure callback) {
  NOTREACHED();
}

void TrustedVaultClientBackend::GetDegradedRecoverabilityStatus(
    ChromeIdentity* chrome_identity,
    base::OnceCallback<void(bool)> callback) {
  GetDegradedRecoverabilityStatus(
      static_cast<id<SystemIdentity>>(chrome_identity), std::move(callback));
}

void TrustedVaultClientBackend::GetDegradedRecoverabilityStatus(
    id<SystemIdentity> identity,
    base::OnceCallback<void(bool)> callback) {
  NOTREACHED();
}

void TrustedVaultClientBackend::Reauthentication(
    ChromeIdentity* chrome_identity,
    UIViewController* presenting_view_controller,
    CompletionBlock callback) {
  Reauthentication(static_cast<id<SystemIdentity>>(chrome_identity),
                   presenting_view_controller, std::move(callback));
}

void TrustedVaultClientBackend::Reauthentication(
    id<SystemIdentity> identity,
    UIViewController* presenting_view_controller,
    CompletionBlock callback) {
  NOTREACHED();
}

void TrustedVaultClientBackend::FixDegradedRecoverability(
    ChromeIdentity* chrome_identity,
    UIViewController* presenting_view_controller,
    CompletionBlock callback) {
  FixDegradedRecoverability(static_cast<id<SystemIdentity>>(chrome_identity),
                            presenting_view_controller, std::move(callback));
}

void TrustedVaultClientBackend::FixDegradedRecoverability(
    id<SystemIdentity> identity,
    UIViewController* presenting_view_controller,
    CompletionBlock callback) {
  NOTREACHED();
}
