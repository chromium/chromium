// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/passkey_model_observer_bridge.h"

PasskeyModelObserverBridge::PasskeyModelObserverBridge(
    id<PasskeyModelObserverDelegate> observer_delegate,
    webauthn::PasskeyModel* passkey_model)
    : observer_(observer_delegate) {
  DCHECK(observer_);

  scoped_observation_.Observe(passkey_model);
}

PasskeyModelObserverBridge::~PasskeyModelObserverBridge() {}

bool PasskeyModelObserverBridge::IsObserving(
    webauthn::PasskeyModel* passkey_model) const {
  return passkey_model == passkey_model_;
}

// webauthn::PasskeyModel::Observer:

void PasskeyModelObserverBridge::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {}

void PasskeyModelObserverBridge::OnPasskeyModelShuttingDown() {
  scoped_observation_.Reset();
}

void PasskeyModelObserverBridge::OnPasskeyModelIsReady(bool is_ready) {
  [observer_ passkeyModelIsReady:passkey_model_];
}
