// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_DESKTOP_SESSION_BACKEND_H_
#define REMOTING_HOST_LINUX_DESKTOP_SESSION_BACKEND_H_

#include <sys/types.h>

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "remoting/base/loggable.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/mojom/desktop_session.mojom-forward.h"

namespace remoting {

class DaemonProcess;

// Interface for Linux desktop session backend implementations.
class DesktopSessionBackend {
 public:
  using Callback = base::OnceCallback<void(base::expected<void, Loggable>)>;

  virtual ~DesktopSessionBackend() = default;

  // Starts the backend. `callback` is called once initialization completes.
  virtual void Start(Callback callback) = 0;

  // Creates a new desktop session.
  virtual std::unique_ptr<DesktopSession> CreateDesktopSession(
      int id,
      DaemonProcess* daemon_process,
      const mojom::DesktopSessionOptions& options) = 0;

  // Terminates all active desktop sessions managed by this backend.
  virtual void TerminateAllSessions(Callback callback) = 0;

  // Finds a DesktopSession with the matching UID. Returns nullptr if not found.
  virtual DesktopSession* GetSessionByUid(uid_t uid) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_DESKTOP_SESSION_BACKEND_H_
