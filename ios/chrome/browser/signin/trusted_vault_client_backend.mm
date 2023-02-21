// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/trusted_vault_client_backend.h"

#import "base/functional/callback.h"
#import "base/notreached.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TrustedVaultClientBackend::TrustedVaultClientBackend() = default;

TrustedVaultClientBackend::~TrustedVaultClientBackend() = default;

void TrustedVaultClientBackend::SetDeviceRegistrationPublicKeyVerifierForUMA(
    VerifierCallback verifier) {
  NOTREACHED();
}

void TrustedVaultClientBackend::ClearLocalData(
    id<SystemIdentity> identity,
    base::OnceCallback<void(bool)> callback) {
  NOTREACHED();
}

void TrustedVaultClientBackend::GetPublicKeyForIdentity(
    id<SystemIdentity> identity,
    GetPublicKeyCallback callback) {
  NOTREACHED();
}
