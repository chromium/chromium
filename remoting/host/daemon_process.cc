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
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/branding.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/protocol/transport.h"

namespace remoting {

namespace {

// This is used for tagging system event logs.
const char kApplicationName[] = "chromoting";

std::ostream& operator<<(std::ostream& os, const ScreenResolution& resolution) {
  return os << resolution.dimensions().width() << "x"
            << resolution.dimensions().height() << " at "
            << resolution.dpi().x() << "x" << resolution.dpi().y() << " DPI";
}

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

  Stop();
}

void DaemonProcess::OnChannelConnected(int32_t peer_pid) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  VLOG(1) << "IPC: daemon <- network (" << peer_pid << ")";

  DeleteAllDesktopSessions();

  // Reset the last known terminal ID because no IDs have been allocated
  // by the the newly started process yet.
  next_terminal_id_ = 0;

  SendHostConfigToNetworkProcess(serialized_config_);
}

void DaemonProcess::OnPermanentError(int exit_code) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  DCHECK(kMinPermanentErrorExitCode <= exit_code &&
         exit_code <= kMaxPermanentErrorExitCode);

  Stop();
}

void DaemonProcess::OnWorkerProcessStopped() {
  desktop_session_manager_.reset();
  host_status_observer_.reset();
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
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // Validate the supplied terminal ID. An attempt to use a desktop session ID
  // that couldn't possibly have been allocated is considered a protocol error
  // and the network process will be restarted.
  if (!WasTerminalIdAllocated(terminal_id)) {
    LOG(ERROR) << "Invalid terminal ID: " << terminal_id;
    CrashNetworkProcess(FROM_HERE);
    return;
  }

  DesktopSessionList::iterator i;
  for (i = desktop_sessions_.begin(); i != desktop_sessions_.end(); ++i) {
    if ((*i)->id() == terminal_id) {
      break;
    }
  }

  // It is OK if the terminal ID wasn't found. There is a race between
  // the network and daemon processes. Each frees its own recources first and
  // notifies the other party if there was something to clean up.
  if (i == desktop_sessions_.end()) {
    return;
  }

  delete *i;
  desktop_sessions_.erase(i);

  VLOG(1) << "Daemon: closed desktop session " << terminal_id;
  SendTerminalDisconnected(terminal_id);
}

DaemonProcess::DaemonProcess(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    base::OnceClosure stopped_callback)
    : caller_task_runner_(caller_task_runner),
      io_task_runner_(io_task_runner),
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

void DaemonProcess::CreateDesktopSession(int terminal_id,
                                         const ScreenResolution& resolution,
                                         bool virtual_terminal) {
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
      DoCreateDesktopSession(terminal_id, resolution, virtual_terminal);
  if (!session) {
    LOG(ERROR) << "Failed to create a desktop session.";
    SendTerminalDisconnected(terminal_id);
    return;
  }

  VLOG(1) << "Daemon: opened desktop session " << terminal_id;
  desktop_sessions_.push_back(session.release());
}

void DaemonProcess::SetScreenResolution(int terminal_id,
                                        const ScreenResolution& resolution) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // Validate the supplied terminal ID. An attempt to use a desktop session ID
  // that couldn't possibly have been allocated is considered a protocol error
  // and the network process will be restarted.
  if (!WasTerminalIdAllocated(terminal_id)) {
    LOG(ERROR) << "Invalid terminal ID: " << terminal_id;
    CrashNetworkProcess(FROM_HERE);
    return;
  }

  // Validate |resolution| and restart the sender if it is not valid.
  if (resolution.IsEmpty()) {
    LOG(ERROR) << "Invalid resolution specified: " << resolution;
    CrashNetworkProcess(FROM_HERE);
    return;
  }

  DesktopSessionList::iterator i;
  for (i = desktop_sessions_.begin(); i != desktop_sessions_.end(); ++i) {
    if ((*i)->id() == terminal_id) {
      break;
    }
  }

  // It is OK if the terminal ID wasn't found. There is a race between
  // the network and daemon processes. Each frees its own resources first and
  // notifies the other party if there was something to clean up.
  if (i == desktop_sessions_.end()) {
    return;
  }

  (*i)->SetScreenResolution(resolution);
}

void DaemonProcess::CrashNetworkProcess(const base::Location& location) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  DoCrashNetworkProcess(location);
  DeleteAllDesktopSessions();
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

void DaemonProcess::Stop() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  OnWorkerProcessStopped();

  if (stopped_callback_) {
    std::move(stopped_callback_).Run();
  }
}

bool DaemonProcess::WasTerminalIdAllocated(int terminal_id) {
  return terminal_id < next_terminal_id_;
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
  while (!desktop_sessions_.empty()) {
    delete desktop_sessions_.front();
    desktop_sessions_.pop_front();
  }
}

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
