// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/daemon_process.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/values.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/branding.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/chromoting_host_services_server.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/peer_connection_process_handler.h"
#include "remoting/protocol/transport.h"

namespace remoting {

namespace {

// This is used for tagging system event logs.
const char kApplicationName[] = "chromoting";

}  // namespace

DaemonProcess::~DaemonProcess() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  host_event_logger_ = nullptr;
  config_watcher_ = nullptr;
  DeleteAllDesktopSessions();
}

void DaemonProcess::OnConfigUpdated(const std::string& serialized_config) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  if (serialized_config_ != serialized_config) {
    serialized_config_ = serialized_config;
    SendHostConfigToNetworkProcess(serialized_config_);
  }
}

void DaemonProcess::OnConfigWatcherError() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  Stop(kInvalidHostConfigurationExitCode);
}

void DaemonProcess::OnChannelConnected(int32_t peer_pid) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  VLOG(1) << "IPC: daemon <- network (" << peer_pid << ")";

  DeleteAllDesktopSessions();

  // Reset the last known terminal ID because no IDs have been allocated
  // by the the newly started process yet.
  next_terminal_id_ = 0;

  BindAssociatedInterfaces();

  if (!OnInitAfterChannelConnected(peer_pid)) {
    return;
  }

  SendHostConfigToNetworkProcess(serialized_config_);
}

void DaemonProcess::OnPermanentError(int exit_code) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  DCHECK(kMinPermanentErrorExitCode <= exit_code &&
         exit_code <= kMaxPermanentErrorExitCode);

  Stop(exit_code);
}

void DaemonProcess::OnWorkerProcessStopped() {
  desktop_session_manager_.reset();
  peer_session_manager_.reset();
  host_status_observer_.reset();
  // Reset our IPC remote so it's ready to re-init if the network process is
  // re-launched.
  remoting_host_control_.reset();
  peer_connection_launchers_.clear();
  DeleteAllDesktopSessions();
}

void DaemonProcess::OnAssociatedInterfaceRequest(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // Typically we'd want to ensure that an associated receiver was not requested
  // multiple times as that would indicate a logic error (or that the calling
  // process had possibly been compromised). In the case of the network process,
  // which handles network traffic and encoding, it's possible that there is a
  // protocol error or OS driver fault which causes the process to crash. When
  // that occurs, the daemon process will launch a new instance of the network
  // process (which is handled outside of this class) and that new instance will
  // attempt to retrieve the set of associated interfaces it needs to do its
  // work. If that occurs, we log a warning and allow the new process to set up
  // its associated remotes. In other areas of the code we might crash the
  // requesting (or current) process but that could lead to a crash loop here.

  if (interface_name == mojom::DesktopSessionManager::Name_) {
    LOG_IF(WARNING, desktop_session_manager_)
        << "Associated interface requested "
        << "while |desktop_session_manager_| was still bound.";

    desktop_session_manager_.reset();
    mojo::PendingAssociatedReceiver<mojom::DesktopSessionManager>
        pending_receiver(std::move(handle));
    desktop_session_manager_.Bind(std::move(pending_receiver));
  } else if (interface_name == mojom::PeerSessionManager::Name_) {
    LOG_IF(WARNING, peer_session_manager_)
        << "Associated interface requested "
        << "while |peer_session_manager_| was still bound.";

    peer_session_manager_.reset();
    mojo::PendingAssociatedReceiver<mojom::PeerSessionManager> pending_receiver(
        std::move(handle));
    peer_session_manager_.Bind(std::move(pending_receiver));
  } else if (interface_name == mojom::HostStatusObserver::Name_) {
    LOG_IF(WARNING, host_status_observer_)
        << "Associated interface requested "
        << "while |host_status_observer_| was still bound.";

    host_status_observer_.reset();
    mojo::PendingAssociatedReceiver<mojom::HostStatusObserver> pending_receiver(
        std::move(handle));
    host_status_observer_.Bind(std::move(pending_receiver));
  } else {
    LOG(ERROR) << "Received unexpected associated interface request: "
               << interface_name;
  }
}

void DaemonProcess::CloseDesktopSession(int terminal_id) {
  CloseDesktopSessionWithError(terminal_id, ErrorCode::OK, {}, FROM_HERE);
}

void DaemonProcess::CloseDesktopSessionWithError(
    int terminal_id,
    ErrorCode error_code,
    const std::string& error_details,
    const SourceLocation& error_location) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // Validate the supplied terminal ID. An attempt to use a desktop session ID
  // that couldn't possibly have been allocated is considered a protocol error
  // and the network process will be restarted.
  if (!WasTerminalIdAllocated(terminal_id)) {
    LOG(ERROR) << "Invalid terminal ID: " << terminal_id;
    CrashNetworkProcess(FROM_HERE);
    return;
  }

  // CloseDesktopSession() can be called multiple times for the same terminal ID
  // because CloseDesktopSession() and OnDesktopSessionAgentDisconnected() can
  // be called concurrently from different threads. It is also called when we
  // close the desktop session ourselves, or receive a request to close it from
  // the network and daemon processes. Each frees its own resources first and
  // notifies the other party if there was something to clean up.
  auto i = desktop_sessions_.find(terminal_id);
  if (i == desktop_sessions_.end()) {
    return;
  }

  SendTerminalDisconnected(terminal_id, error_code, error_details,
                           error_location);

  delete i->second;
  desktop_sessions_.erase(i);

  VLOG(1) << "Daemon: closed desktop session " << terminal_id;
}

void DaemonProcess::LaunchPeerSession(
    mojo::PendingReceiver<mojom::PeerSession> peer_session_receiver) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // TODO(crbug.com/502281489): Investigate process auto-relaunch suppression
  // and session reconnection semantics when a Peer Connection process stops.
  PeerConnectionProcessHandler* handler = LaunchPeerConnectionProcess();
  if (handler) {
    handler->BindPeerSession(std::move(peer_session_receiver));
  }
}

PeerConnectionProcessHandler* DaemonProcess::LaunchPeerConnectionProcess() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  auto delegate = CreatePeerConnectionProcessLauncherDelegate();
  if (!delegate) {
    LOG(ERROR) << "Failed to create launcher delegate.";
    return nullptr;
  }

  // Safe to use base::Unretained(this) because DaemonProcess owns the
  // PeerConnectionProcessHandler instances (in peer_connection_launchers_)
  // which outlive the lifetime of the callbacks.
  auto handler = std::make_unique<PeerConnectionProcessHandler>(
      caller_task_runner(), std::move(delegate),
      base::BindOnce(&DaemonProcess::OnPeerConnectionProcessStopped,
                     base::Unretained(this)));
  PeerConnectionProcessHandler* handler_ptr = handler.get();
  peer_connection_launchers_.insert(std::move(handler));
  return handler_ptr;
}

void DaemonProcess::OnPeerConnectionProcessStopped(
    base::WeakPtr<PeerConnectionProcessHandler> handler) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  // If the handler is invalid, it was already destroyed and removed from
  // `peer_connection_launchers_` (e.g. during daemon shutdown).
  if (!handler) {
    return;
  }
  auto it = peer_connection_launchers_.find(handler.get());
  if (it != peer_connection_launchers_.end()) {
    peer_connection_launchers_.erase(it);
  }
}

DaemonProcess::DaemonProcess(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    StoppedCallback stopped_callback)
    : caller_task_runner_(caller_task_runner),
      io_task_runner_(io_task_runner),
      ipc_support_(io_task_runner->task_runner(),
                   mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST),
      next_terminal_id_(0),
      stopped_callback_(std::move(stopped_callback)),
      status_monitor_(new HostStatusMonitor()) {
  DCHECK(caller_task_runner->BelongsToCurrentThread());
  // TODO(sammc): On OSX, mojo::core::SetMachPortProvider() should be called
  // with a base::PortProvider implementation. Add it here when this code is
  // used on OSX.

  // Tests may use their own thread pool so create one if needed.
  if (!base::ThreadPoolInstance::Get()) {
    base::ThreadPoolInstance::CreateAndStartWithDefaultParams("Daemon");
  }
}

void DaemonProcess::GetDesktopSession(
    mojo::PendingReceiver<mojom::DesktopSession> control_receiver,
    mojo::PendingRemote<mojom::DesktopSessionEvents> events_remote,
    mojom::DesktopSessionOptionsPtr options) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  std::string client_id = options->client_id;
  auto it = std::ranges::find_if(desktop_sessions_, [&](const auto& pair) {
    return !client_id.empty() && pair.second->client_id() == client_id;
  });

  if (it != desktop_sessions_.end()) {
    int terminal_id = it->first;
    ReconnectDesktopSession(terminal_id, std::move(control_receiver),
                            std::move(events_remote), std::move(options));
    return;
  }

  int terminal_id = next_terminal_id_;
  CreateDesktopSession(terminal_id, std::move(control_receiver),
                       std::move(events_remote), std::move(options));
}

void DaemonProcess::CreateDesktopSession(
    int terminal_id,
    mojo::PendingReceiver<mojom::DesktopSession> control_receiver,
    mojo::PendingRemote<mojom::DesktopSessionEvents> events_remote,
    mojom::DesktopSessionOptionsPtr options) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // Validate the supplied terminal ID. An attempt to create a desktop session
  // with an ID that could possibly have been allocated already is considered
  // a protocol error and the network process will be restarted.
  if (WasTerminalIdAllocated(terminal_id)) {
    LOG(ERROR) << "Invalid terminal ID: " << terminal_id;
    CrashNetworkProcess(FROM_HERE);
    return;
  }

  // Terminal IDs cannot be reused. Update the expected next terminal ID.
  next_terminal_id_ = std::max(next_terminal_id_, terminal_id + 1);

  // Create the desktop session.
  std::unique_ptr<DesktopSession> session =
      DoCreateDesktopSession(terminal_id, *options);
  if (!session) {
    LOG(ERROR) << "Failed to create a desktop session.";
    if (events_remote.is_valid()) {
      mojo::Remote<mojom::DesktopSessionEvents>(std::move(events_remote))
          ->OnTerminalDisconnected(ErrorCode::HOST_CONFIGURATION_ERROR,
                                   "Failed to create a desktop session.",
                                   FROM_HERE);
    }
    return;
  }

  session->set_client_id(options->client_id);
  session->SetReceiver(std::move(control_receiver));
  session->SetEventsRemote(std::move(events_remote));
  VLOG(1) << "Daemon: opened desktop session " << terminal_id;
  desktop_sessions_[terminal_id] = session.release();
}

void DaemonProcess::ReconnectDesktopSession(
    int terminal_id,
    mojo::PendingReceiver<mojom::DesktopSession> control_receiver,
    mojo::PendingRemote<mojom::DesktopSessionEvents> events_remote,
    mojom::DesktopSessionOptionsPtr options) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  auto it = desktop_sessions_.find(terminal_id);

  if (it == desktop_sessions_.end()) {
    LOG(ERROR) << "Invalid terminal ID: " << terminal_id;
    CrashNetworkProcess(FROM_HERE);
    return;
  }
  VLOG(1) << "Daemon: reconnecting desktop session " << terminal_id;
  it->second->SetReceiver(std::move(control_receiver));
  it->second->SetEventsRemote(std::move(events_remote));
  it->second->ReconnectNetworkChannel(*options);
}

void DaemonProcess::CrashNetworkProcess(const base::Location& location) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  DoCrashNetworkProcess(location);
  DeleteAllDesktopSessions();
}

void DaemonProcess::DoCrashNetworkProcess(const base::Location& location) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  if (network_launcher_) {
    network_launcher_->Crash(location);
  }
}

void DaemonProcess::SetNetworkLauncherDelegate(
    std::unique_ptr<WorkerProcessLauncher::Delegate> delegate) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  DCHECK(!network_launcher_);
  network_launcher_ =
      std::make_unique<WorkerProcessLauncher>(std::move(delegate), this);
}

bool DaemonProcess::OnDesktopSessionAgentAttached(
    int terminal_id,
    mojo::ScopedMessagePipeHandle desktop_pipe) {
  auto session_it = desktop_sessions_.find(terminal_id);

  if (session_it != desktop_sessions_.end() &&
      session_it->second->events_remote()) {
    session_it->second->events_remote()->OnDesktopSessionAgentAttached(
        std::move(desktop_pipe));
  }

  return true;
}

void DaemonProcess::SendTerminalDisconnected(
    int terminal_id,
    ErrorCode error_code,
    const std::string& error_details,
    const SourceLocation& error_location) {
  auto session_it = desktop_sessions_.find(terminal_id);

  if (session_it != desktop_sessions_.end() &&
      session_it->second->events_remote()) {
    session_it->second->events_remote()->OnTerminalDisconnected(
        error_code, error_details, error_location);
  }
}

void DaemonProcess::SendHostConfigToNetworkProcess(
    const std::string& serialized_config) {
  if (!remoting_host_control_) {
    return;
  }

  LOG_IF(ERROR, !remoting_host_control_.is_connected())
      << "IPC channel not connected. HostConfig message will be dropped.";

  std::optional<base::DictValue> config(HostConfigFromJson(serialized_config));
  if (!config.has_value()) {
    LOG(ERROR) << "Invalid host config, shutting down.";
    OnPermanentError(kInvalidHostConfigurationExitCode);
    return;
  }

  remoting_host_control_->ApplyHostConfig(std::move(config.value()));
}

void DaemonProcess::BindAssociatedInterfaces() {
  if (!network_launcher_) {
    return;
  }
  // Typically the Daemon process is responsible for disconnecting the remote
  // however in cases where the network process crashes, we want to ensure that
  // |remoting_host_control_| is reset so it can be reused after the network
  // process is relaunched.
  remoting_host_control_.reset();
  network_launcher_->GetRemoteAssociatedInterface(
      remoting_host_control_.BindNewEndpointAndPassReceiver());
}

bool DaemonProcess::OnInitAfterChannelConnected(int32_t peer_pid) {
  return true;
}

void DaemonProcess::Cleanup(base::OnceClosure callback) {
  std::move(callback).Run();
}

void DaemonProcess::Initialize() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  config_watcher_ = std::make_unique<ConfigFileWatcher>(
      caller_task_runner(), io_task_runner(), GetConfigPath());
  config_watcher_->Watch(this);
  host_event_logger_ =
      HostEventLogger::Create(status_monitor_, kApplicationName);

  StartChromotingHostServices();

  // Launch the process.
  LaunchNetworkProcess();
}

void DaemonProcess::Stop(int exit_code) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  OnWorkerProcessStopped();

  if (stopped_callback_) {
    std::move(stopped_callback_).Run(exit_code);
  }
}

bool DaemonProcess::WasTerminalIdAllocated(int terminal_id) {
  return terminal_id < next_terminal_id_;
}

void DaemonProcess::StartChromotingHostServices() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  DCHECK(!host_services_server_);

  host_services_server_ =
      std::make_unique<ChromotingHostServicesServer>(base::BindRepeating(
          &DaemonProcess::BindChromotingHostServices, base::Unretained(this)));
  host_services_server_->StartServer();
  HOST_LOG << "ChromotingHostServices IPC server has been started.";
}

void DaemonProcess::BindChromotingHostServices(
    mojo::PendingReceiver<mojom::ChromotingHostServices> receiver,
    std::unique_ptr<named_mojo_ipc_server::ConnectionInfo> connection_info) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  if (!connection_info) {
    LOG(WARNING) << "Binding rejected because no connection info was provided.";
    return;
  }
  host_services_receivers_.Add(this, std::move(receiver),
                               std::move(connection_info));
}

void DaemonProcess::OnClientAccessDenied(const std::string& signaling_id) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  for (auto& observer : status_monitor_->observers()) {
    observer.OnClientAccessDenied(signaling_id);
  }
}

void DaemonProcess::OnClientAuthenticated(const std::string& signaling_id) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  for (auto& observer : status_monitor_->observers()) {
    observer.OnClientAuthenticated(signaling_id);
  }
}

void DaemonProcess::OnClientConnected(const std::string& signaling_id) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  for (auto& observer : status_monitor_->observers()) {
    observer.OnClientConnected(signaling_id);
  }
}

void DaemonProcess::OnClientDisconnected(const std::string& signaling_id) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  for (auto& observer : status_monitor_->observers()) {
    observer.OnClientDisconnected(signaling_id);
  }
}

void DaemonProcess::OnClientRouteChange(const std::string& signaling_id,
                                        const std::string& channel_name,
                                        const protocol::TransportRoute& route) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  for (auto& observer : status_monitor_->observers()) {
    observer.OnClientRouteChange(signaling_id, channel_name, route);
  }
}

void DaemonProcess::OnHostStarted(const std::string& owner_email) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  for (auto& observer : status_monitor_->observers()) {
    observer.OnHostStarted(owner_email);
  }
}

void DaemonProcess::OnHostShutdown() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  for (auto& observer : status_monitor_->observers()) {
    observer.OnHostShutdown();
  }
}

void DaemonProcess::DeleteAllDesktopSessions() {
  for (auto& [id, session] : desktop_sessions_) {
    delete session;
  }
  desktop_sessions_.clear();
}

// static
base::FilePath DaemonProcess::GetConfigPath() {
  base::FilePath config_path;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kHostConfigSwitchName)) {
    config_path = command_line->GetSwitchValuePath(kHostConfigSwitchName);
  } else {
    base::FilePath default_config_dir = remoting::GetConfigDir();
    config_path = default_config_dir.Append(kDefaultHostConfigFile);
  }
  return config_path;
}

}  // namespace remoting
