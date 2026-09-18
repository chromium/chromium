// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GDM_MANAGED_DESKTOP_SESSION_BACKEND_H_
#define REMOTING_HOST_LINUX_GDM_MANAGED_DESKTOP_SESSION_BACKEND_H_

#include <sys/types.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/files/file_error_or.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "remoting/base/loggable.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/linux/desktop_session_backend.h"
#include "remoting/host/linux/remote_display_session_manager.h"
#include "remoting/host/mojom/desktop_session.mojom-forward.h"

namespace remoting {

class DaemonProcess;
class DesktopSessionLinux;

// Desktop session backend for GDM-managed sessions. Associates each desktop
// session with a GDM remote display, systemd login session, and desktop
// process.
class GdmManagedDesktopSessionBackend final
    : public DesktopSessionBackend,
      public RemoteDisplaySessionManager::Delegate {
 public:
  explicit GdmManagedDesktopSessionBackend(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~GdmManagedDesktopSessionBackend() override;

  GdmManagedDesktopSessionBackend(const GdmManagedDesktopSessionBackend&) =
      delete;
  GdmManagedDesktopSessionBackend& operator=(
      const GdmManagedDesktopSessionBackend&) = delete;

  // DesktopSessionBackend implementation.
  void Start(Callback callback) override;
  std::unique_ptr<DesktopSession> CreateDesktopSession(
      int id,
      DaemonProcess* daemon_process,
      const mojom::DesktopSessionOptions& options) override;
  void TerminateAllSessions(Callback callback) override;
  DesktopSession* GetSessionByUid(uid_t uid) override;

 private:
  void OnStartResult(Callback callback, base::expected<void, Loggable> result);

  void OnRemoteDisplaysFileLoaded(Callback callback,
                                  base::FileErrorOr<std::string> load_result);

  void OnCreateRemoteDisplayResult(std::string_view display_name,
                                   base::expected<void, Loggable> result);

  // RemoteDisplaySessionManager::Delegate implementation.
  void OnRemoteDisplayChanged(
      std::string_view display_name,
      const RemoteDisplaySessionManager::RemoteDisplayInfo& info) override;
  void OnRemoteDisplayTerminated(std::string_view display_name) override;

  // Removes `display_name` from `desktop_sessions_` and terminates the remote
  // display.
  void RemoveDesktopSession(std::string_view display_name);

  // Finds a DesktopSessionLinux with the display name. Returns nullptr if not
  // found.
  base::WeakPtr<DesktopSessionLinux> FindSession(std::string_view display_name);

  // Requests that the remote displays file be written to disk. The write is
  // throttled by a timer.
  void RequestWriteRemoteDisplaysToFile();

  // Actually writes the remote displays to disk.
  void WriteRemoteDisplaysToFile();

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);
  RemoteDisplaySessionManager remote_display_session_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::RetainingOneShotTimer write_remote_displays_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Note that sessions that have been terminated but not yet destroyed are
  // still in this map.
  base::flat_map<std::string /*display_name*/,
                 base::WeakPtr<DesktopSessionLinux>>
      desktop_sessions_ GUARDED_BY_CONTEXT(sequence_checker_);

  // A collection of remote displays that are recovered from the previous CRD
  // host incarnation.
  base::flat_map<std::string /*client_id*/, std::string /*display_name*/>
      recovered_displays_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<GdmManagedDesktopSessionBackend> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GDM_MANAGED_DESKTOP_SESSION_BACKEND_H_
