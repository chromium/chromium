// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_STATE_CHANGE_NOTIFIER_H_
#define REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_STATE_CHANGE_NOTIFIER_H_

namespace remoting {

// Interface to notify possible changes in the state of whether WebAuthn
// proxying is allowed in the current desktop session.
class RemoteWebAuthnStateChangeNotifier {
 public:
  virtual ~RemoteWebAuthnStateChangeNotifier() = default;

  // Notifies that the remote WebAuthn state has possibly changed. Safe to call
  // this method when the state has not changed.
  virtual void NotifyStateChange() = 0;

 protected:
  RemoteWebAuthnStateChangeNotifier() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_STATE_CHANGE_NOTIFIER_H_
