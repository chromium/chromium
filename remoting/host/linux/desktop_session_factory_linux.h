// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_DESKTOP_SESSION_FACTORY_LINUX_H_
#define REMOTING_HOST_LINUX_DESKTOP_SESSION_FACTORY_LINUX_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/daemon_process.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/linux/remote_display_session_manager.h"

namespace remoting {

class DaemonProcess;
class DesktopSession;

// Class to create DesktopSessions and the corresponding GDM remote displays and
// desktop processes. Each DesktopSession is associated with one GDM remote
// display, one systemd login session, and one desktop process.
//
// This class requires the current process to be run as root.
class DesktopSessionFactoryLinux final
    : public RemoteDisplaySessionManager::Delegate {
 public:
  using Callback = base::OnceCallback<void(base::expected<void, Loggable>)>;

  explicit DesktopSessionFactoryLinux(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~DesktopSessionFactoryLinux() override;

  DesktopSessionFactoryLinux(const DesktopSessionFactoryLinux&) = delete;
  DesktopSessionFactoryLinux& operator=(const DesktopSessionFactoryLinux&) =
      delete;

  // Starts the factory. Must be called exactly once before calling other
  // methods. `callback` is called once the factory has successfully started or
  // failed to start.
  void Start(Callback callback);

  // Creates a new desktop session instance.
  std::unique_ptr<DesktopSession> CreateDesktopSession(
      int id,
      DaemonProcess* daemon_process,
      const mojom::DesktopSessionOptions& options);

 private:
  class DesktopSessionLinux;

  void OnStartResult(Callback callback, base::expected<void, Loggable> result);

  void OnCreateRemoteDisplayResult(std::string_view display_name,
                                   base::expected<void, Loggable> result);

  // RemoteDisplaySessionManager::Delegate implementation.
  void OnRemoteDisplaySessionChanged(
      std::string_view display_name,
      const RemoteDisplaySessionManager::RemoteDisplayInfo& info) override;
  void OnRemoteDisplayTerminated(std::string_view display_name) override;

  // Removes `display_name` from `desktop_sessions_` and terminates the remote
  // display.
  void RemoveDesktopSession(std::string_view display_name);

  // Finds a DesktopSession with the display name. Return nullptr if not found.
  base::WeakPtr<DesktopSessionLinux> FindSession(std::string_view display_name);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);
  RemoteDisplaySessionManager remote_display_session_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Note that sessions that have been terminated but not yet destroyed are
  // still in this map.
  base::flat_map<std::string /*display_name*/,
                 base::WeakPtr<DesktopSessionLinux>>
      desktop_sessions_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<DesktopSessionFactoryLinux> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_DESKTOP_SESSION_FACTORY_LINUX_H_
