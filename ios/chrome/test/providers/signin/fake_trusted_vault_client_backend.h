// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_SIGNIN_FAKE_TRUSTED_VAULT_CLIENT_BACKEND_H_
#define IOS_CHROME_TEST_PROVIDERS_SIGNIN_FAKE_TRUSTED_VAULT_CLIENT_BACKEND_H_

#include "ios/chrome/browser/signin/model/trusted_vault_client_backend.h"

@class FakeTrustedVaultClientBackendViewController;

// A fake implementation of TrustedVaultClientBackend API for tests.
class FakeTrustedVaultClientBackend final : public TrustedVaultClientBackend {
 public:
  FakeTrustedVaultClientBackend();
  ~FakeTrustedVaultClientBackend() final;

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

  // Simulates user cancelling the reauth dialog.
  void SimulateUserCancel();

 private:
  // Dismisses the `view_controller_`.
  void InternalCancelDialog(BOOL animated, ProceduralBlock callback);

  FakeTrustedVaultClientBackendViewController* view_controller_ = nil;

  base::WeakPtrFactory<FakeTrustedVaultClientBackend> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_TEST_PROVIDERS_SIGNIN_FAKE_TRUSTED_VAULT_CLIENT_BACKEND_H_
