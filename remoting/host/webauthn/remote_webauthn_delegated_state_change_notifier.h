// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_DELEGATED_STATE_CHANGE_NOTIFIER_H_
#define REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_DELEGATED_STATE_CHANGE_NOTIFIER_H_

#include "base/functional/callback.h"
#include "remoting/host/webauthn/remote_webauthn_state_change_notifier.h"

namespace remoting {

// A RemoteWebAuthnStateChangeNotifier implementation that simply calls
// |notify_state_change| when NotifyStateChange() is called.
class RemoteWebAuthnDelegatedStateChangeNotifier
    : public RemoteWebAuthnStateChangeNotifier {
 public:
  explicit RemoteWebAuthnDelegatedStateChangeNotifier(
      const base::RepeatingClosure& notify_state_change);
  ~RemoteWebAuthnDelegatedStateChangeNotifier() override;

  void NotifyStateChange() override;

 protected:
  base::RepeatingClosure notify_state_change_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_DELEGATED_STATE_CHANGE_NOTIFIER_H_
