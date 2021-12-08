// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_EXTENSION_NOTIFIER_H_
#define REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_EXTENSION_NOTIFIER_H_

namespace remoting {

// Class to notify the remote WebAuthn proxy extension of possible changes in
// the state of whether WebAuthn proxying is allowed in the current desktop
// session.
class RemoteWebAuthnExtensionNotifier final {
 public:
  RemoteWebAuthnExtensionNotifier();
  RemoteWebAuthnExtensionNotifier(const RemoteWebAuthnExtensionNotifier&) =
      delete;
  RemoteWebAuthnExtensionNotifier& operator=(
      const RemoteWebAuthnExtensionNotifier&) = delete;
  ~RemoteWebAuthnExtensionNotifier();

  // Notifies the extension that the remote WebAuthn state has possibly changed.
  // Safe to call this method when the state has not changed.
  void NotifyStateChange();
};

}  // namespace remoting

#endif  // REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_EXTENSION_NOTIFIER_H_
