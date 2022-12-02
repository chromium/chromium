// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_EXTENSION_NOTIFIER_H_
#define REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_EXTENSION_NOTIFIER_H_

#include "remoting/host/webauthn/remote_webauthn_state_change_notifier.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"

namespace remoting {

// Class to notify the remote WebAuthn proxy extension of possible changes in
// the state of whether WebAuthn proxying is allowed in the current desktop
// session.
class RemoteWebAuthnExtensionNotifier final
    : public RemoteWebAuthnStateChangeNotifier {
 public:
  static const std::vector<base::FilePath::StringType>&
  GetRemoteWebAuthnExtensionIds();

  RemoteWebAuthnExtensionNotifier();
  RemoteWebAuthnExtensionNotifier(const RemoteWebAuthnExtensionNotifier&) =
      delete;
  RemoteWebAuthnExtensionNotifier& operator=(
      const RemoteWebAuthnExtensionNotifier&) = delete;
  ~RemoteWebAuthnExtensionNotifier() override;

  void NotifyStateChange() override;

 private:
  friend class RemoteWebAuthnExtensionNotifierTest;
  class Core;

  void WakeUpExtension();

  RemoteWebAuthnExtensionNotifier(
      std::vector<base::FilePath> possible_remote_state_change_directories,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  base::SequenceBound<Core> core_;
  bool is_wake_up_scheduled_ = false;
  base::WeakPtrFactory<RemoteWebAuthnExtensionNotifier> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_EXTENSION_NOTIFIER_H_
