// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_TRUSTED_VAULT_CLIENT_BACKEND_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_TRUSTED_VAULT_CLIENT_BACKEND_H_

#include <UIKit/UIKit.h>

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/ios/block_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/trusted_vault/trusted_vault_client.h"

@protocol SystemIdentity;

// Abstract class to manage shared keys.
class TrustedVaultClientBackend : public KeyedService {
 public:
  // Helper types representing a key and a list of key respectively.
  using SharedKey = std::vector<uint8_t>;
  using SharedKeyList = std::vector<SharedKey>;

  // A public key.
  using PublicKey = std::vector<uint8_t>;

  // Represents the TrustedVaultClientBackend observers.
  using Observer = trusted_vault::TrustedVaultClient::Observer;

  // Types for the different callbacks.
  using KeyFetchedCallback = base::OnceCallback<void(const SharedKeyList&)>;
  using CompletionBlock = void (^)(BOOL success, NSError* error);
  using GetPublicKeyCallback = base::OnceCallback<void(const PublicKey&)>;

  // Callback used to verify local device registration and log the result to
  // UMA metrics. The argument represents the gaia ID subject to verification.
  using VerifierCallback = base::OnceCallback<void(const std::string&)>;
  TrustedVaultClientBackend();

  TrustedVaultClientBackend(const TrustedVaultClientBackend&) = delete;
  TrustedVaultClientBackend& operator=(const TrustedVaultClientBackend&) =
      delete;

  ~TrustedVaultClientBackend() override;

  // Adds/removes observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Registers a delegate-like callback that implements device registration
  // verification.
  virtual void SetDeviceRegistrationPublicKeyVerifierForUMA(
      VerifierCallback verifier) = 0;

  // Asynchronously fetches the shared keys for `identity` and invokes
  // `callback` with the fetched keys.
  virtual void FetchKeys(id<SystemIdentity> identity,
                         KeyFetchedCallback callback) = 0;

  // Invoked when the result of FetchKeys() contains keys that are not
  // up-to-date. During the execution, before `callback` is invoked, the
  // behavior is unspecified if FetchKeys() is invoked, that is, FetchKeys()
  // may or may not treat existing keys as stale (only guaranteed upon
  // completion of MarkLocalKeysAsStale()).
  virtual void MarkLocalKeysAsStale(id<SystemIdentity> identity,
                                    base::OnceClosure callback) = 0;

  // Returns whether recoverability of the keys is degraded and user action is
  // required to add a new method.
  virtual void GetDegradedRecoverabilityStatus(
      id<SystemIdentity> identity,
      base::OnceCallback<void(bool)> callback) = 0;

  // Presents the trusted vault key reauthentication UI for `identity` for the
  // purpose of extending the set of keys returned via FetchKeys(). Once the
  // reauth is done and the UI is dismissed, `callback` is called. `callback` is
  // not called if the reauthentication is canceled.
  virtual void Reauthentication(id<SystemIdentity> identity,
                                UIViewController* presenting_view_controller,
                                CompletionBlock callback) = 0;

  // Presents the trusted vault key reauthentication UI for `identity` for the
  // purpose of improving recoverability as returned via
  // GetDegradedRecoverabilityStatus(). Once the reauth is done and the UI is
  // dismissed, `callback` is called. `callback` is not called if the
  // reauthentication is canceled.
  virtual void FixDegradedRecoverability(
      id<SystemIdentity> identity,
      UIViewController* presenting_view_controller,
      CompletionBlock callback) = 0;

  // Cancels the presented trusted vault reauthentication UI, triggered via
  // either Reauthentication() or via
  // FixDegradedRecoverability(). The reauthentication callback
  // will not be called. If no reauthentication dialog is not present,
  // `callback` is called synchronously.
  virtual void CancelDialog(BOOL animated, ProceduralBlock callback) = 0;

  // Clears local data belonging to `identity`, such as shared keys. This
  // excludes the physical client's key pair, which remains unchanged.
  virtual void ClearLocalData(id<SystemIdentity> identity,
                              base::OnceCallback<void(bool)> callback) = 0;

  // Returns the member public key used to enroll the local device.
  virtual void GetPublicKeyForIdentity(id<SystemIdentity> identity,
                                       GetPublicKeyCallback callback) = 0;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_TRUSTED_VAULT_CLIENT_BACKEND_H_
