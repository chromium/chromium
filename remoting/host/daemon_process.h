// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DAEMON_PROCESS_H_
#define REMOTING_HOST_DAEMON_PROCESS_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/host/config_watcher.h"
#include "remoting/host/host_status_monitor.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/worker_process_ipc_delegate.h"

namespace base {
class Location;
}  // namespace base

namespace remoting {

class AutoThreadTaskRunner;
class DesktopSession;
class HostEventLogger;
class ScreenResolution;

// This class implements core of the daemon process. It manages the networking
// process running at lower privileges and maintains the list of desktop
// sessions.
class DaemonProcess : public ConfigWatcher::Delegate,
                      public WorkerProcessIpcDelegate,
                      public HostStatusObserver,
                      public mojom::DesktopSessionManager {
 public:
  typedef std::list<raw_ptr<DesktopSession, CtnExperimental>>
      DesktopSessionList;

  DaemonProcess(const DaemonProcess&) = delete;
  DaemonProcess& operator=(const DaemonProcess&) = delete;

  ~DaemonProcess() override;

  // Creates a platform-specific implementation of the daemon process object
  // passing relevant task runners. Public methods of this class must be called
  // on the |caller_task_runner| thread. |io_task_runner| is used to handle IPC
  // and background I/O tasks.
  static std::unique_ptr<DaemonProcess> Create(
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner,
      const base::OnceClosure stopped_callback);

  // ConfigWatcher::Delegate
  void OnConfigUpdated(const std::string& serialized_config) override;
  void OnConfigWatcherError() override;

  scoped_refptr<HostStatusMonitor> status_monitor() { return status_monitor_; }

  // WorkerProcessIpcDelegate implementation.
  void OnChannelConnected(int32_t peer_pid) override;
  void OnPermanentError(int exit_code) override;
  void OnWorkerProcessStopped() override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // mojom::DesktopSessionManager implementation.
  void CreateDesktopSession(int terminal_id,
                            const ScreenResolution& resolution,
                            bool virtual_terminal) override;
  void CloseDesktopSession(int terminal_id) override;
  void SetScreenResolution(int terminal_id,
                           const ScreenResolution& resolution) override;

  // Called when a desktop integration process attaches to |terminal_id|.
  // |session_id| is the id of the desktop session being attached.
  // |desktop_pipe| specifies the client end of the desktop pipe. Returns true
  // on success, false otherwise.
  virtual bool OnDesktopSessionAgentAttached(
      int terminal_id,
      int session_id,
      mojo::ScopedMessagePipeHandle desktop_pipe) = 0;

  // Requests the network process to crash.
  void CrashNetworkProcess(const base::Location& location);

 protected:
  DaemonProcess(scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
                scoped_refptr<AutoThreadTaskRunner> io_task_runner,
                base::OnceClosure stopped_callback);

  // Reads the host configuration and launches the network process.
  void Initialize();

  // Invokes |stopped_callback_| to ask the owner to delete |this|.
  void Stop();

  // Returns true if |terminal_id| is in the range of allocated IDs. I.e. it is
  // less or equal to the highest ID we have seen so far.
  bool WasTerminalIdAllocated(int terminal_id);

  // HostStatusObserver overrides.
  void OnClientAccessDenied(const std::string& signaling_id) override;
  void OnClientAuthenticated(const std::string& signaling_id) override;
  void OnClientConnected(const std::string& signaling_id) override;
  void OnClientDisconnected(const std::string& signaling_id) override;
  void OnClientRouteChange(const std::string& signaling_id,
                           const std::string& channel_name,
                           const protocol::TransportRoute& route) override;
  void OnHostStarted(const std::string& owner_email) override;
  void OnHostShutdown() override;

  // Creates a platform-specific desktop session and assigns a unique ID to it.
  // An implementation should validate |params| as they are received via IPC.
  virtual std::unique_ptr<DesktopSession> DoCreateDesktopSession(
      int terminal_id,
      const ScreenResolution& resolution,
      bool virtual_terminal) = 0;

  // Requests the network process to crash.
  virtual void DoCrashNetworkProcess(const base::Location& location) = 0;

  // Launches the network process and establishes an IPC channel with it.
  virtual void LaunchNetworkProcess() = 0;

  // Sends |serialized_config| to the network process. The config includes
  // details such as the host owner email and robot account refresh token which
  // are required to start the host and get online.
  virtual void SendHostConfigToNetworkProcess(
      const std::string& serialized_config) = 0;

  // Notifies the network process that the daemon has disconnected the desktop
  // session from the associated desktop environment.
  virtual void SendTerminalDisconnected(int terminal_id) = 0;

  virtual void StartChromotingHostServices() = 0;

  scoped_refptr<AutoThreadTaskRunner> caller_task_runner() {
    return caller_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> io_task_runner() {
    return io_task_runner_;
  }

  // Let the test code analyze the list of desktop sessions.
  friend class DaemonProcessTest;
  const DesktopSessionList& desktop_sessions() const {
    return desktop_sessions_;
  }

 private:
  // Deletes all desktop sessions.
  void DeleteAllDesktopSessions();

  // Gets the location of the config file.
  base::FilePath GetConfigPath();

  // Task runner on which public methods of this class must be called.
  scoped_refptr<AutoThreadTaskRunner> caller_task_runner_;

  // Handles IPC and background I/O tasks.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  std::unique_ptr<ConfigWatcher> config_watcher_;

  // The configuration file contents.
  std::string serialized_config_;

  // The list of active desktop sessions.
  DesktopSessionList desktop_sessions_;

  // The highest desktop session ID that has been seen so far.
  int next_terminal_id_;

  // Invoked to ask the owner to delete |this|.
  base::OnceClosure stopped_callback_;

  // Writes host status updates to the system event log.
  std::unique_ptr<HostEventLogger> host_event_logger_;

  mojo::AssociatedReceiver<mojom::DesktopSessionManager>
      desktop_session_manager_{this};
  mojo::AssociatedReceiver<mojom::HostStatusObserver> host_status_observer_{
      this};

  scoped_refptr<HostStatusMonitor> status_monitor_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DAEMON_PROCESS_H_
