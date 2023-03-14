// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_SIGNIN_FAKE_TRUSTED_VAULT_CLIENT_BACKEND_H_
#define IOS_CHROME_TEST_PROVIDERS_SIGNIN_FAKE_TRUSTED_VAULT_CLIENT_BACKEND_H_

#include "ios/chrome/browser/signin/trusted_vault_client_backend.h"

@class FakeTrustedVaultClientBackendViewController;

// A fake implementation of TrustedVaultClientBackend API for tests.
class FakeTrustedVaultClientBackend final : public TrustedVaultClientBackend {
 public:
  FakeTrustedVaultClientBackend();
  ~FakeTrustedVaultClientBackend() final;

  // TrustedVaultClientBackend implementation.
  void AddObserver(Observer* observer) final;
  void RemoveObserver(Observer* observer) final;
  void SetDeviceRegistrationPublicKeyVerifierForUMA(
      VerifierCallback verifier) final;
  void FetchKeys(id<SystemIdentity> identity,
                 KeyFetchedCallback callback) final;
  void MarkLocalKeysAsStale(id<SystemIdentity> identity,
                            base::OnceClosure callback) final;
  void GetDegradedRecoverabilityStatus(
      id<SystemIdentity> identity,
      base::OnceCallback<void(bool)> callback) final;
  void Reauthentication(id<SystemIdentity> identity,
                        UIViewController* presenting_view_controller,
                        CompletionBlock callback) final;
  void FixDegradedRecoverability(id<SystemIdentity> identity,
                                 UIViewController* presenting_view_controller,
                                 CompletionBlock callback) final;
  void CancelDialog(BOOL animated, ProceduralBlock callback) final;
  void ClearLocalData(id<SystemIdentity> identity,
                      base::OnceCallback<void(bool)> callback) final;
  void GetPublicKeyForIdentity(id<SystemIdentity> identity,
                               GetPublicKeyCallback callback) final;

  // Simulates user cancelling the reauth dialog.
  void SimulateUserCancel();

 private:
  FakeTrustedVaultClientBackendViewController* view_controller_ = nil;
};

#endif  // IOS_CHROME_TEST_PROVIDERS_SIGNIN_FAKE_TRUSTED_VAULT_CLIENT_BACKEND_H_
