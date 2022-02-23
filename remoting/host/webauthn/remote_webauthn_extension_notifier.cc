// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_extension_notifier.h"

#include "base/notreached.h"

namespace remoting {

RemoteWebAuthnExtensionNotifier::RemoteWebAuthnExtensionNotifier() = default;

RemoteWebAuthnExtensionNotifier::~RemoteWebAuthnExtensionNotifier() = default;

void RemoteWebAuthnExtensionNotifier::NotifyStateChange() {
  // TODO(yuweih): Replace with real implementation. Also NotifyStateChange()
  // might get called multiple times in the same turn of the message loop, so
  // the calls should be deduplicated by posting a task to the task runner.
  NOTIMPLEMENTED();
}

}  // namespace remoting
