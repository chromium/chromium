// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DAEMON_PROCESS_H_
#define REMOTING_HOST_DAEMON_PROCESS_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/errors.h"
#include "remoting/base/source_location.h"
#include "remoting/host/config_watcher.h"
#include "remoting/host/host_status_monitor.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/mojom/remoting_host.mojom.h"
#include "remoting/host/worker_process_ipc_delegate.h"
#include "remoting/host/worker_process_launcher.h"

namespace base {
class Location;
}  // namespace base

namespace named_mojo_ipc_server {
struct ConnectionInfo;
}

namespace remoting {

class ChromotingHostServicesServer;
class DesktopSession;
class PeerConnectionProcessHandler;
class HostEventLogger;
class ScreenResolution;

// This class implements core of the daemon process. It manages the networking
// process running at lower privileges and maintains the list of desktop
// sessions.
class DaemonProcess : public ConfigWatcher::Delegate,
                      public WorkerProcessIpcDelegate,
                      public HostStatusObserver,
                      public mojom::DesktopSessionManager,
                      public mojom::ChromotingHostServices {
 public:
  using StoppedCallback = base::OnceCallback<void(int /*exit_code*/)>;
  using DesktopSessionList =
      base::circular_deque<raw_ptr<DesktopSession, CtnExperimental>>;

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
      StoppedCallback stopped_callback);

  // Gets the location of the config file.
  static base::FilePath GetConfigPath();

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
                            mojom::DesktopSessionOptionsPtr options) override;
  void ReconnectDesktopSession(
      int terminal_id,
      mojom::DesktopSessionOptionsPtr options) override;
  void CloseDesktopSession(int terminal_id) override;
  void CloseDesktopSessionWithError(int terminal_id,
                                    ErrorCode error_code,
                                    const std::string& error_details,
                                    const SourceLocation& error_location);
  void SetScreenResolution(int terminal_id,
                           const ScreenResolution& resolution) override;

  // Called when a desktop integration process attaches to |terminal_id|.
  // |desktop_pipe| specifies the client end of the desktop pipe. Returns true
  // on success, false otherwise.
  virtual bool OnDesktopSessionAgentAttached(
      int terminal_id,
      mojo::ScopedMessagePipeHandle desktop_pipe);

  // Requests the network process to crash.
  void CrashNetworkProcess(const base::Location& location);

  // Called whenever the daemon process is asked to terminate gracefully. The
  // implementation may cleanup resources such as closing desktop sessions.
  // `callback` is called once the cleanup has complete.
  virtual void Cleanup(base::OnceClosure callback);

 protected:
  DaemonProcess(scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
                scoped_refptr<AutoThreadTaskRunner> io_task_runner,
                StoppedCallback stopped_callback);

  // Reads the host configuration and launches the network process.
  void Initialize();

  // Invokes |stopped_callback_| to ask the owner to delete |this|.
  void Stop(int exit_code);

  // Returns true if |terminal_id| is in the range of allocated IDs. I.e. it is
  // less or equal to the highest ID we have seen so far.
  bool WasTerminalIdAllocated(int terminal_id);

  void StartChromotingHostServices();

  void BindChromotingHostServices(
      mojo::PendingReceiver<mojom::ChromotingHostServices> receiver,
      std::unique_ptr<named_mojo_ipc_server::ConnectionInfo> connection_info);

  // mojom::ChromotingHostServices implementation.
  void BindSessionServices(
      mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver)
      override = 0;

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
      const mojom::DesktopSessionOptions& options) = 0;

  // Launches the network process and establishes an IPC channel with it.
  virtual void LaunchNetworkProcess() = 0;

  // Platform-specific initialization after the IPC channel is connected.
  virtual bool OnInitAfterChannelConnected(int32_t peer_pid);

  // Factory method implemented by platform subclasses to create their
  // specific launcher delegate.
  virtual std::unique_ptr<WorkerProcessLauncher::Delegate>
  CreatePeerConnectionProcessLauncherDelegate(int terminal_id) = 0;

  // Virtual for testing.
  virtual void SendHostConfigToNetworkProcess(
      const std::string& serialized_config);

  // Virtual for testing.
  virtual void SendTerminalDisconnected(int terminal_id,
                                        ErrorCode error_code,
                                        const std::string& error_details,
                                        const SourceLocation& error_location);

  // Requests the network process to crash. Virtual for testing.
  virtual void DoCrashNetworkProcess(const base::Location& location);

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

  mojo::ReceiverSet<mojom::ChromotingHostServices,
                    std::unique_ptr<named_mojo_ipc_server::ConnectionInfo>>&
  host_services_receivers() {
    return host_services_receivers_;
  }

  bool IsNetworkProcessReady() const {
    return remoting_host_control_.is_bound() &&
           desktop_session_connection_events_.is_bound();
  }

  void SetNetworkLauncherDelegate(
      std::unique_ptr<WorkerProcessLauncher::Delegate> delegate);

  mojom::RemotingHostControl* remoting_host_control() {
    return remoting_host_control_.get();
  }

  mojom::DesktopSessionConnectionEvents* desktop_session_connection_events() {
    return desktop_session_connection_events_.get();
  }

 private:
  // Launches the peer connection process for |terminal_id| and establishes an
  // IPC channel with it.
  void LaunchPeerConnectionProcess(int terminal_id);

  // Closes the peer connection process for |terminal_id|.
  void ClosePeerConnectionProcess(int terminal_id);

  // Tracks active peer connection process launchers. The keys are
  // `terminal_id`.
  std::map<int, std::unique_ptr<PeerConnectionProcessHandler>>
      peer_connection_launchers_;

  // Binds associated interfaces to the network process launcher.
  void BindAssociatedInterfaces();

  // Deletes all desktop sessions.
  void DeleteAllDesktopSessions();

  // Task runner on which public methods of this class must be called.
  scoped_refptr<AutoThreadTaskRunner> caller_task_runner_;

  // Handles IPC and background I/O tasks.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  // Mojo keeps the task runner passed to it alive forever, so an
  // AutoThreadTaskRunner should not be passed to it. Otherwise, the process may
  // never shut down cleanly.
  mojo::core::ScopedIPCSupport ipc_support_;

  std::unique_ptr<WorkerProcessLauncher> network_launcher_;

  mojo::AssociatedRemote<mojom::DesktopSessionConnectionEvents>
      desktop_session_connection_events_;
  mojo::AssociatedRemote<mojom::RemotingHostControl> remoting_host_control_;

  std::unique_ptr<ConfigWatcher> config_watcher_;

  // The configuration file contents.
  std::string serialized_config_;

  // The list of active desktop sessions.
  DesktopSessionList desktop_sessions_;

  // The highest desktop session ID that has been seen so far.
  int next_terminal_id_;

  // Invoked to ask the owner to delete |this|.
  StoppedCallback stopped_callback_;

  // Writes host status updates to the system event log.
  std::unique_ptr<HostEventLogger> host_event_logger_;

  mojo::AssociatedReceiver<mojom::DesktopSessionManager>
      desktop_session_manager_{this};
  mojo::AssociatedReceiver<mojom::HostStatusObserver> host_status_observer_{
      this};

  scoped_refptr<HostStatusMonitor> status_monitor_;

  mojo::ReceiverSet<mojom::ChromotingHostServices,
                    std::unique_ptr<named_mojo_ipc_server::ConnectionInfo>>
      host_services_receivers_;

  std::unique_ptr<ChromotingHostServicesServer> host_services_server_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DAEMON_PROCESS_H_
