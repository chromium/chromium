// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_TRUSTED_VAULT_SERVICE_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_TRUSTED_VAULT_SERVICE_H_

#include <CoreFoundation/CoreFoundation.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/observer_list.h"
#include "components/sync/driver/trusted_vault_client.h"

@class ChromeIdentity;
@class UIViewController;

namespace ios {

using TrustedVaultSharedKey = std::vector<uint8_t>;
using TrustedVaultSharedKeyList = std::vector<TrustedVaultSharedKey>;

// Abstract class to manage shared keys.
class ChromeTrustedVaultService {
 public:
  ChromeTrustedVaultService();
  ChromeTrustedVaultService(const ChromeTrustedVaultService&) = delete;
  virtual ~ChromeTrustedVaultService();
  ChromeTrustedVaultService& operator=(const ChromeTrustedVaultService&) =
      delete;

  using Observer = syncer::TrustedVaultClient::Observer;

  // Adds/removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Asynchronously fetch the shared keys for |identity|
  // and returns them by calling |callback|.
  virtual void FetchKeys(
      ChromeIdentity* chrome_identity,
      base::OnceCallback<void(const TrustedVaultSharedKeyList&)> callback) = 0;

  // Invoked when the result of FetchKeys() contains keys that are not
  // up-to-date. During the execution, before |cb| is invoked, the behavior is
  // unspecified if FetchKeys() is invoked, that is, FetchKeys() may or may not
  // treat existing keys as stale (only guaranteed upon completion of
  // MarkLocalKeysAsStale()).
  virtual void MarkLocalKeysAsStale(ChromeIdentity* chrome_identity,
                                    base::OnceClosure callback) = 0;

  // Returns whether recoverability of the keys is degraded and user action is
  // required to add a new method.
  virtual void GetDegradedRecoverabilityStatus(
      ChromeIdentity* chrome_identity,
      base::OnceCallback<void(bool)> callback) = 0;

  // Presents the trusted vault key reauthentication UI for |identity| for the
  // purpose of extending the set of keys returned via FetchKeys(). Once the
  // reauth is done and the UI is dismissed, |callback| is called. |callback| is
  // not called if the reauthentication is canceled.
  virtual void Reauthentication(ChromeIdentity* chrome_identity,
                                UIViewController* presentingViewController,
                                void (^callback)(BOOL success,
                                                 NSError* error)) = 0;

  // Presents the trusted vault key reauthentication UI for |identity| for the
  // purpose of improving recoverability as returned via
  // GetDegradedRecoverabilityStatus(). Once the reauth is done and the UI is
  // dismissed, |callback| is called. |callback| is not called if the
  // reauthentication is canceled.
  virtual void FixDegradedRecoverability(
      ChromeIdentity* chrome_identity,
      UIViewController* presentingViewController,
      void (^callback)(BOOL success, NSError* error)) = 0;

  // Presents the trusted vault key reauthentication UI for |identity| for the
  // purpose of opting into trusted vault passphrase. Once the reauth is done
  // and the UI is dismissed, |callback| is called. |callback| is not called if
  // the reauthentication is canceled.
  // TODO(crbug.com/1202088): Make pure.
  virtual void ReauthenticationForOptIn(
      ChromeIdentity* chrome_identity,
      UIViewController* presentingViewController,
      void (^callback)(BOOL success, NSError* error));

  // Cancels the presented trusted vault reauthentication UI, triggered via
  // either Reauthentication() or via
  // FixDegradedRecoverability(). The reauthentication callback
  // will not be called. If no reauthentication dialog is not present,
  // |callback| is called synchronously.
  virtual void CancelDialog(BOOL animated, void (^callback)(void)) = 0;

  // Clears local data belonging for the security domain and identity, such as
  // shared keys. Data not specific for a specific domain (e.g. private key) is
  // not removed. |identity| The identity for which the domain-specific local
  // data is cleared. |callback| Block called when clearing data is complete.
  virtual void ClearLocalDataForIdentity(ChromeIdentity* chrome_identity,
                                         void (^callback)(BOOL success,
                                                          NSError* error)) = 0;

 protected:
  // Functions to notify observers.
  void NotifyKeysChanged();
  void NotifyRecoverabilityChanged();

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_TRUSTED_VAULT_SERVICE_H_
