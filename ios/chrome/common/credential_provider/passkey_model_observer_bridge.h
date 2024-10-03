// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_PASSKEY_MODEL_OBSERVER_BRIDGE_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_PASSKEY_MODEL_OBSERVER_BRIDGE_H_

#import "base/scoped_observation.h"
#import "components/webauthn/core/browser/passkey_model.h"

@protocol PasskeyModelObserverDelegate
- (void)passkeyModelIsReady:(webauthn::PasskeyModel*)passkeyModel;
@end

// This class observes a passkey model which is not ready yet with the sole
// purpose of calling the provided callback once the passkey model becomes
// ready.
class PasskeyModelObserverBridge : public webauthn::PasskeyModel::Observer {
 public:
  PasskeyModelObserverBridge(id<PasskeyModelObserverDelegate> delegate,
                             webauthn::PasskeyModel* passkey_model);
  ~PasskeyModelObserverBridge() override;

  bool IsObserving(webauthn::PasskeyModel* passkey_model) const;

 private:
  // webauthn::PasskeyModel::Observer:
  void OnPasskeysChanged(
      const std::vector<webauthn::PasskeyModelChange>& changes) override;
  void OnPasskeyModelShuttingDown() override;
  void OnPasskeyModelIsReady(bool is_ready) override;

  // The passkey model being observed.
  raw_ptr<webauthn::PasskeyModel> passkey_model_;

  __weak id<PasskeyModelObserverDelegate> observer_;
  base::ScopedObservation<webauthn::PasskeyModel,
                          webauthn::PasskeyModel::Observer>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_PASSKEY_MODEL_OBSERVER_BRIDGE_H_
