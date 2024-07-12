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
  void AddObserver(Observer* observer) final;
  void RemoveObserver(Observer* observer) final;
  void SetDeviceRegistrationPublicKeyVerifierForUMA(
      VerifierCallback verifier) final;
  void FetchKeys(id<SystemIdentity> identity,
                 NSString* security_domain,
                 KeyFetchedCallback completion) final;
  void MarkLocalKeysAsStale(id<SystemIdentity> identity,
                            NSString* security_domain,
                            base::OnceClosure completion) final;
  void GetDegradedRecoverabilityStatus(
      id<SystemIdentity> identity,
      NSString* security_domain,
      base::OnceCallback<void(bool)> completion) final;
  void Reauthentication(id<SystemIdentity> identity,
                        NSString* security_domain,
                        UIViewController* presenting_view_controller,
                        CompletionBlock completion) final;
  void FixDegradedRecoverability(id<SystemIdentity> identity,
                                 NSString* security_domain,
                                 UIViewController* presenting_view_controller,
                                 CompletionBlock completion) final;
  void CancelDialog(BOOL animated, ProceduralBlock callback) final;
  void ClearLocalData(id<SystemIdentity> identity,
                      NSString* security_domain,
                      base::OnceCallback<void(bool)> completion) final;
  void GetPublicKeyForIdentity(id<SystemIdentity> identity,
                               GetPublicKeyCallback completion) final;

  // Simulates user cancelling the reauth dialog.
  void SimulateUserCancel();

 private:
  FakeTrustedVaultClientBackendViewController* view_controller_ = nil;
};

#endif  // IOS_CHROME_TEST_PROVIDERS_SIGNIN_FAKE_TRUSTED_VAULT_CLIENT_BACKEND_H_
