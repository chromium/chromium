// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/trusted_vault_client_backend.h"

#import "base/functional/callback.h"

TrustedVaultClientBackend::TrustedVaultClientBackend() = default;

TrustedVaultClientBackend::~TrustedVaultClientBackend() = default;

TrustedVaultClientBackend::CancelDialogCallback
TrustedVaultClientBackend::Reauthentication(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  return ReauthenticationWithCancelCallback(
      identity, security_domain_path, presenting_view_controller, completion);
}

TrustedVaultClientBackend::CancelDialogCallback
TrustedVaultClientBackend::ReauthenticationWithCancelCallback(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  NOTREACHED_NORETURN();
}

TrustedVaultClientBackend::CancelDialogCallback
TrustedVaultClientBackend::FixDegradedRecoverability(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  return FixDegradedRecoverabilityWithCancelCallback(
      identity, security_domain_path, presenting_view_controller, completion);
}

TrustedVaultClientBackend::CancelDialogCallback
TrustedVaultClientBackend::FixDegradedRecoverabilityWithCancelCallback(
    id<SystemIdentity> identity,
    const std::string& security_domain_path,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  NOTREACHED_NORETURN();
}
