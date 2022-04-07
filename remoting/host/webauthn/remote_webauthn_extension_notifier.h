// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_EXTENSION_NOTIFIER_H_
#define REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_EXTENSION_NOTIFIER_H_

#include "remoting/host/webauthn/remote_webauthn_state_change_notifier.h"

namespace remoting {

// Class to notify the remote WebAuthn proxy extension of possible changes in
// the state of whether WebAuthn proxying is allowed in the current desktop
// session.
class RemoteWebAuthnExtensionNotifier final
    : public RemoteWebAuthnStateChangeNotifier {
 public:
  RemoteWebAuthnExtensionNotifier();
  RemoteWebAuthnExtensionNotifier(const RemoteWebAuthnExtensionNotifier&) =
      delete;
  RemoteWebAuthnExtensionNotifier& operator=(
      const RemoteWebAuthnExtensionNotifier&) = delete;
  ~RemoteWebAuthnExtensionNotifier() override;

  void NotifyStateChange() override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_EXTENSION_NOTIFIER_H_
