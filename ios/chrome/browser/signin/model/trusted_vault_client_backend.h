// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_TRUSTED_VAULT_CLIENT_BACKEND_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_TRUSTED_VAULT_CLIENT_BACKEND_H_

#import <UIKit/UIKit.h>

#import <map>
#import <string>
#import <vector>

#import "base/functional/callback_forward.h"
#import "base/ios/block_types.h"
#import "base/observer_list.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/trusted_vault/trusted_vault_client.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"

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
  using CancelDialogCallback =
      base::OnceCallback<void(BOOL animated, ProceduralBlock cancel_done)>;

  // Callback used to verify local device registration and log the result to
  // UMA metrics. The argument represents the gaia ID subject to verification.
  using VerifierCallback = base::OnceCallback<void(const std::string&)>;
  TrustedVaultClientBackend();

  TrustedVaultClientBackend(const TrustedVaultClientBackend&) = delete;
  TrustedVaultClientBackend& operator=(const TrustedVaultClientBackend&) =
      delete;

  ~TrustedVaultClientBackend() override;

  // Adds/removes observers.
  void AddObserver(Observer* observer,
                   trusted_vault::SecurityDomainId security_domain_id);
  void RemoveObserver(Observer* observer,
                      trusted_vault::SecurityDomainId security_domain_id);

  // Registers a delegate-like callback that implements device registration
  // verification.
  // TODO(crbug.com/40939090): device registration verification has been
  // removed, remove remaining code from TrustedVaultClientBackend.
  virtual void SetDeviceRegistrationPublicKeyVerifierForUMA(
      VerifierCallback verifier) = 0;

  // Asynchronously fetches the shared keys for `identity` and invokes
  // `callback` with the fetched keys.
  virtual void FetchKeys(id<SystemIdentity> identity,
                         trusted_vault::SecurityDomainId security_domain_id,
                         KeyFetchedCallback completion) = 0;

  // Invoked when the result of FetchKeys() contains keys that are not
  // up-to-date. During the execution, before `callback` is invoked, the
  // behavior is unspecified if FetchKeys() is invoked, that is, FetchKeys()
  // may or may not treat existing keys as stale (only guaranteed upon
  // completion of MarkLocalKeysAsStale()).
  virtual void MarkLocalKeysAsStale(
      id<SystemIdentity> identity,
      trusted_vault::SecurityDomainId security_domain_id,
      base::OnceClosure completion) = 0;

  // Returns whether recoverability of the keys is degraded and user action is
  // required to add a new method.
  virtual void GetDegradedRecoverabilityStatus(
      id<SystemIdentity> identity,
      trusted_vault::SecurityDomainId security_domain_id,
      base::OnceCallback<void(bool)> completion) = 0;

  // Presents the trusted vault key reauthentication UI for `identity` for the
  // purpose of extending the set of keys returned via FetchKeys(). Once the
  // reauth is done and the UI is dismissed, `completion` is called.
  // `completion` is not called if the reauthentication is canceled.
  virtual CancelDialogCallback Reauthentication(
      id<SystemIdentity> identity,
      trusted_vault::SecurityDomainId security_domain_id,
      UIViewController* presenting_view_controller,
      CompletionBlock completion) = 0;

  // Presents the trusted vault key reauthentication UI for `identity` for the
  // purpose of improving recoverability as returned via
  // GetDegradedRecoverabilityStatus(). Once the reauth is done and the UI is
  // dismissed, `completion` is called. `completion` is not called if the
  // reauthentication is canceled.
  virtual CancelDialogCallback FixDegradedRecoverability(
      id<SystemIdentity> identity,
      trusted_vault::SecurityDomainId security_domain_id,
      UIViewController* presenting_view_controller,
      CompletionBlock completion) = 0;

  // Clears local data belonging to `identity`, such as shared keys. This
  // excludes the physical client's key pair, which remains unchanged.
  virtual void ClearLocalData(
      id<SystemIdentity> identity,
      trusted_vault::SecurityDomainId security_domain_id,
      base::OnceCallback<void(bool)> completion) = 0;

  // Returns the member public key used to enroll the local device.
  virtual void GetPublicKeyForIdentity(id<SystemIdentity> identity,
                                       GetPublicKeyCallback callback) = 0;

 protected:
  // Functions to notify observers.
  void NotifyKeysChanged(trusted_vault::SecurityDomainId security_domain_id);

  void NotifyRecoverabilityChanged(
      trusted_vault::SecurityDomainId security_domain_id);

 private:
  // List of observers per security domain path.
  std::map<trusted_vault::SecurityDomainId, base::ObserverList<Observer>>
      observer_lists_per_security_domain_id_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_TRUSTED_VAULT_CLIENT_BACKEND_H_
