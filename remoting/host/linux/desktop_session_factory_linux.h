// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_DESKTOP_SESSION_FACTORY_LINUX_H_
#define REMOTING_HOST_LINUX_DESKTOP_SESSION_FACTORY_LINUX_H_

#include <sys/types.h>

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "remoting/base/loggable.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/linux/desktop_session_backend.h"
#include "remoting/host/linux/session_routing_config.h"
#include "remoting/host/mojom/desktop_session.mojom-forward.h"

namespace remoting {

class DaemonProcess;

// Facade for creating DesktopSessions. Delegates session management to an
// underlying DesktopSessionBackend implementation (e.g.
// GdmManagedDesktopSessionBackend).
//
// This class requires the current process to be run as root.
class DesktopSessionFactoryLinux final {
 public:
  using Callback = base::OnceCallback<void(base::expected<void, Loggable>)>;

  explicit DesktopSessionFactoryLinux(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~DesktopSessionFactoryLinux();

  DesktopSessionFactoryLinux(const DesktopSessionFactoryLinux&) = delete;
  DesktopSessionFactoryLinux& operator=(const DesktopSessionFactoryLinux&) =
      delete;

  // Starts the factory. Must be called exactly once before calling other
  // methods. `callback` is called once the factory has successfully started or
  // failed to start.
  void Start(bool is_corp_host, Callback callback);

  // Creates a new desktop session instance.
  std::unique_ptr<DesktopSession> CreateDesktopSession(
      int id,
      DaemonProcess* daemon_process,
      const mojom::DesktopSessionOptions& options);

  // Terminates all active desktop sessions.
  void TerminateAllSessions(Callback callback);

  // Finds a DesktopSession with the matching UID. Returns nullptr if not found.
  DesktopSession* GetSessionByUid(uid_t uid);

 private:
  void OnSessionRoutingConfigLoaded(
      Callback callback,
      base::expected<std::optional<SessionRoutingConfig>, Loggable> result);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<DesktopSessionBackend> backend_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<DesktopSessionFactoryLinux> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_DESKTOP_SESSION_FACTORY_LINUX_H_
