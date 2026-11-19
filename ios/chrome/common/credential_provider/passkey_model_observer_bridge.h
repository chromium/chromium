// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_PASSKEY_MODEL_OBSERVER_BRIDGE_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_PASSKEY_MODEL_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/scoped_observation.h"
#import "components/webauthn/core/browser/passkey_model.h"

// Delegate receiving observation events from `webauthn::PasskeyModel`.
@protocol PasskeyModelObserverDelegate <NSObject>

// Notifies the delegate that `passkeyModel` is shutting down.
- (void)passKeyModelShuttingDown:(webauthn::PasskeyModel*)passkeyModel;
// Notifies the delegate that `passkeyModel` loaded state from disk and is ready
// to sync.
- (void)passkeyModelIsReady:(webauthn::PasskeyModel*)passkeyModel;
// Notifies the delegate that the stored passkeys changed.
- (void)passkeyModelDidChange;

@optional
// Notifies the delegate that `passkeyModel` failed initialization and will not
// become ready.
- (void)passkeyModelDidFailInitialization:(webauthn::PasskeyModel*)passkeyModel;

@end

// Objective-C bridge forwarding `webauthn::PasskeyModel::Observer` events to
// `PasskeyModelObserverDelegate`.
class PasskeyModelObserverBridge : public webauthn::PasskeyModel::Observer {
 public:
  PasskeyModelObserverBridge(id<PasskeyModelObserverDelegate> delegate,
                             webauthn::PasskeyModel* passkey_model);
  ~PasskeyModelObserverBridge() override;

  // Returns true if this bridge is currently observing `passkey_model`.
  bool IsObserving(webauthn::PasskeyModel* passkey_model) const;

 private:
  // webauthn::PasskeyModel::Observer:
  void OnPasskeysChanged(
      const std::vector<webauthn::PasskeyModelChange>& changes) override;
  void OnPasskeyModelShuttingDown() override;
  void OnPasskeyModelIsReady(bool is_ready) override;

  __weak id<PasskeyModelObserverDelegate> observer_;
  base::ScopedObservation<webauthn::PasskeyModel,
                          webauthn::PasskeyModel::Observer>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_PASSKEY_MODEL_OBSERVER_BRIDGE_H_
