// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/trusted_vault_api.h"

#import "base/functional/callback.h"
#import "base/notreached.h"

namespace ios {
namespace provider {
namespace {

// A null implementation of TrustedVaultClient used for the public builds. It
// fails all method calls (as it should only be called after the user has been
// signed-in which is not supported by public build).
class ChromiumTrustedVaultClientBackend final
    : public TrustedVaultClientBackend {
 public:
  // TrustedVaultClientBackend implementation.
  void SetDeviceRegistrationPublicKeyVerifierForUMA(
      VerifierCallback verifier) final;
  void FetchKeys(id<SystemIdentity> identity,
                 trusted_vault::SecurityDomainId security_domain_id,
                 KeyFetchedCallback completion) final;
  void MarkLocalKeysAsStale(id<SystemIdentity> identity,
                            trusted_vault::SecurityDomainId security_domain_id,
                            base::OnceClosure completion) final;
  void GetDegradedRecoverabilityStatus(
      id<SystemIdentity> identity,
      trusted_vault::SecurityDomainId security_domain_id,
      base::OnceCallback<void(bool)> completion) final;
  CancelDialogCallback Reauthentication(
      id<SystemIdentity> identity,
      trusted_vault::SecurityDomainId security_domain_id,
      UIViewController* presenting_view_controller,
      CompletionBlock completion) final;
  CancelDialogCallback FixDegradedRecoverability(
      id<SystemIdentity> identity,
      trusted_vault::SecurityDomainId security_domain_id,
      UIViewController* presenting_view_controller,
      CompletionBlock completion) final;
  void ClearLocalData(id<SystemIdentity> identity,
                      trusted_vault::SecurityDomainId security_domain_id,
                      base::OnceCallback<void(bool)> completion) final;
  void GetPublicKeyForIdentity(id<SystemIdentity> identity,
                               GetPublicKeyCallback completion) final;
};

void ChromiumTrustedVaultClientBackend::
    SetDeviceRegistrationPublicKeyVerifierForUMA(VerifierCallback verifier) {
  // Do nothing.
}

void ChromiumTrustedVaultClientBackend::FetchKeys(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    KeyFetchedCallback completion) {
  NOTREACHED();
}

void ChromiumTrustedVaultClientBackend::MarkLocalKeysAsStale(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    base::OnceClosure completion) {
  NOTREACHED();
}

void ChromiumTrustedVaultClientBackend::GetDegradedRecoverabilityStatus(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    base::OnceCallback<void(bool)> completion) {
  NOTREACHED();
}

TrustedVaultClientBackend::CancelDialogCallback
ChromiumTrustedVaultClientBackend::Reauthentication(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  NOTREACHED();
}

TrustedVaultClientBackend::CancelDialogCallback
ChromiumTrustedVaultClientBackend::FixDegradedRecoverability(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    UIViewController* presenting_view_controller,
    CompletionBlock completion) {
  NOTREACHED();
}

void ChromiumTrustedVaultClientBackend::ClearLocalData(
    id<SystemIdentity> identity,
    trusted_vault::SecurityDomainId security_domain_id,
    base::OnceCallback<void(bool)> completion) {
  // Do nothing.
}

void ChromiumTrustedVaultClientBackend::GetPublicKeyForIdentity(
    id<SystemIdentity> identity,
    GetPublicKeyCallback completion) {
  NOTREACHED();
}

}  // anonymous namespace

std::unique_ptr<TrustedVaultClientBackend> CreateTrustedVaultClientBackend(
    TrustedVaultConfiguration* configuration) {
  return std::make_unique<ChromiumTrustedVaultClientBackend>();
}

}  // namespace provider
}  // namespace ios
