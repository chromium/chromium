// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/passkey_model_observer_bridge.h"

PasskeyModelObserverBridge::PasskeyModelObserverBridge(
    id<PasskeyModelObserverDelegate> delegate,
    webauthn::PasskeyModel* passkey_model)
    : observer_(delegate) {
  DCHECK(observer_);

  scoped_observation_.Observe(passkey_model);
}

PasskeyModelObserverBridge::~PasskeyModelObserverBridge() = default;

bool PasskeyModelObserverBridge::IsObserving(
    webauthn::PasskeyModel* passkey_model) const {
  return passkey_model && scoped_observation_.IsObservingSource(passkey_model);
}

// webauthn::PasskeyModel::Observer:

void PasskeyModelObserverBridge::OnPasskeysChanged(
    const std::vector<webauthn::PasskeyModelChange>& changes) {
  [observer_ passkeyModelDidChange];
}

void PasskeyModelObserverBridge::OnPasskeyModelShuttingDown() {
  webauthn::PasskeyModel* passkey_model = scoped_observation_.GetSource();
  scoped_observation_.Reset();
  [observer_ passKeyModelShuttingDown:passkey_model];
}

void PasskeyModelObserverBridge::OnPasskeyModelIsReady(bool is_ready) {
  webauthn::PasskeyModel* passkey_model = scoped_observation_.GetSource();
  if (is_ready) {
    [observer_ passkeyModelIsReady:passkey_model];
    return;
  }
  if ([observer_
          respondsToSelector:@selector(passkeyModelDidFailInitialization:)]) {
    [observer_ passkeyModelDidFailInitialization:passkey_model];
  }
}
