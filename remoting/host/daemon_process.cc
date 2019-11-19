// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/daemon_process.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/constants.h"
#include "remoting/host/branding.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/process_stats_sender.h"
#include "remoting/host/screen_resolution.h"
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
    SendToNetwork(
        new ChromotingDaemonNetworkMsg_Configuration(serialized_config_));
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

  // Send the configuration to the network process.
  SendToNetwork(
      new ChromotingDaemonNetworkMsg_Configuration(serialized_config_));
}

bool DaemonProcess::OnMessageReceived(const IPC::Message& message) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DaemonProcess, message)
    IPC_MESSAGE_HANDLER(ChromotingNetworkHostMsg_ConnectTerminal,
                        CreateDesktopSession)
    IPC_MESSAGE_HANDLER(ChromotingNetworkHostMsg_DisconnectTerminal,
                        CloseDesktopSession)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_SetScreenResolution,
                        SetScreenResolution)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_AccessDenied,
                        OnAccessDenied)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_ClientAuthenticated,
                        OnClientAuthenticated)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_ClientConnected,
                        OnClientConnected)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_ClientDisconnected,
                        OnClientDisconnected)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_ClientRouteChange,
                        OnClientRouteChange)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_HostStarted,
                        OnHostStarted)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_HostShutdown,
                        OnHostShutdown)
    IPC_MESSAGE_HANDLER(ChromotingNetworkToAnyMsg_StartProcessStatsReport,
                        StartProcessStatsReport)
    IPC_MESSAGE_HANDLER(ChromotingNetworkToAnyMsg_StopProcessStatsReport,
                        StopProcessStatsReport)
    IPC_MESSAGE_HANDLER(ChromotingNetworkDaemonMsg_UpdateConfigRefreshToken,
                        UpdateConfigRefreshToken)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled) {
    LOG(ERROR) << "Received unexpected IPC type: " << message.type();
    CrashNetworkProcess(FROM_HERE);
  }

  return handled;
}

void DaemonProcess::OnPermanentError(int exit_code) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  DCHECK(kMinPermanentErrorExitCode <= exit_code &&
         exit_code <= kMaxPermanentErrorExitCode);

  Stop();
}

void DaemonProcess::OnWorkerProcessStopped() {
  process_stats_request_count_ = 0;
  stats_sender_.reset();
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
  if (i == desktop_sessions_.end())
    return;

  delete *i;
  desktop_sessions_.erase(i);

  VLOG(1) << "Daemon: closed desktop session " << terminal_id;
  SendToNetwork(
      new ChromotingDaemonNetworkMsg_TerminalDisconnected(terminal_id));
}

DaemonProcess::DaemonProcess(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    const base::Closure& stopped_callback)
    : caller_task_runner_(caller_task_runner),
      io_task_runner_(io_task_runner),
      next_terminal_id_(0),
      stopped_callback_(stopped_callback),
      status_monitor_(new HostStatusMonitor()),
      current_process_stats_("DaemonProcess") {
  DCHECK(caller_task_runner->BelongsToCurrentThread());
  // TODO(sammc): On OSX, mojo::core::SetMachPortProvider() should be called
  // with a base::PortProvider implementation. Add it here when this code is
  // used on OSX.
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
    SendToNetwork(
        new ChromotingDaemonNetworkMsg_TerminalDisconnected(terminal_id));
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
  if (i == desktop_sessions_.end())
    return;

  (*i)->SetScreenResolution(resolution);
}

void DaemonProcess::CrashNetworkProcess(const base::Location& location) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  DoCrashNetworkProcess(location);
  DeleteAllDesktopSessions();
}

void DaemonProcess::Initialize() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  config_watcher_.reset(new ConfigFileWatcher(
      caller_task_runner(), io_task_runner(), GetConfigPath()));
  config_watcher_->Watch(this);
  host_event_logger_ =
      HostEventLogger::Create(status_monitor_, kApplicationName);

  // Launch the process.
  LaunchNetworkProcess();
}

void DaemonProcess::Stop() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  OnWorkerProcessStopped();

  if (!stopped_callback_.is_null()) {
    std::move(stopped_callback_).Run();
  }
}

bool DaemonProcess::WasTerminalIdAllocated(int terminal_id) {
  return terminal_id < next_terminal_id_;
}

void DaemonProcess::OnAccessDenied(const std::string& jid) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  for (auto& observer : status_observers_)
    observer.OnAccessDenied(jid);
}

void DaemonProcess::OnClientAuthenticated(const std::string& jid) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  for (auto& observer : status_observers_)
    observer.OnClientAuthenticated(jid);
}

void DaemonProcess::OnClientConnected(const std::string& jid) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  for (auto& observer : status_observers_)
    observer.OnClientConnected(jid);
}

void DaemonProcess::OnClientDisconnected(const std::string& jid) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  for (auto& observer : status_observers_)
    observer.OnClientDisconnected(jid);
}

void DaemonProcess::OnClientRouteChange(const std::string& jid,
                                        const std::string& channel_name,
                                        const SerializedTransportRoute& route) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  protocol::TransportRoute parsed_route;
  parsed_route.type = route.type;

  net::IPAddress remote_ip(route.remote_ip.data(), route.remote_ip.size());
  CHECK(remote_ip.empty() || remote_ip.IsValid());
  parsed_route.remote_address = net::IPEndPoint(remote_ip, route.remote_port);

  net::IPAddress local_ip(route.local_ip.data(), route.local_ip.size());
  CHECK(local_ip.empty() || local_ip.IsValid());
  parsed_route.local_address = net::IPEndPoint(local_ip, route.local_port);

  for (auto& observer : status_observers_)
    observer.OnClientRouteChange(jid, channel_name, parsed_route);
}

void DaemonProcess::OnHostStarted(const std::string& xmpp_login) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  for (auto& observer : status_observers_)
    observer.OnStart(xmpp_login);
}

void DaemonProcess::OnHostShutdown() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  for (auto& observer : status_observers_)
    observer.OnShutdown();
}

void DaemonProcess::DeleteAllDesktopSessions() {
  while (!desktop_sessions_.empty()) {
    delete desktop_sessions_.front();
    desktop_sessions_.pop_front();
  }
}

void DaemonProcess::StartProcessStatsReport(base::TimeDelta interval) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  if (interval <= base::TimeDelta::FromSeconds(0)) {
    interval = kDefaultProcessStatsInterval;
  }

  process_stats_request_count_++;
  DCHECK_GT(process_stats_request_count_, 0);
  if (process_stats_request_count_ == 1 ||
      stats_sender_->interval() > interval) {
    DCHECK_EQ(process_stats_request_count_ == 1, !stats_sender_);
    stats_sender_.reset(new ProcessStatsSender(
        this,
        interval,
        { &current_process_stats_ }));
  }
}

void DaemonProcess::StopProcessStatsReport() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  process_stats_request_count_--;
  DCHECK_GE(process_stats_request_count_, 0);
  if (process_stats_request_count_ == 0) {
    DCHECK(stats_sender_);
    stats_sender_.reset();
  }
}

void DaemonProcess::OnProcessStats(
    const protocol::AggregatedProcessResourceUsage& usage) {
  SendToNetwork(new ChromotingAnyToNetworkMsg_ReportProcessStats(usage));
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

void DaemonProcess::UpdateConfigRefreshToken(const std::string& token) {
  io_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&UpdateConfigRefreshTokenOnIoThread,
                                           GetConfigPath(), token));
}

void DaemonProcess::UpdateConfigRefreshTokenOnIoThread(
    const base::FilePath& config_file,
    const std::string& token) {
  std::unique_ptr<base::DictionaryValue> config =
      HostConfigFromJsonFile(config_file);
  if (!config) {
    LOG(ERROR) << "Failed to read config file for updating.";
    return;
  }
  config->SetString(kOAuthRefreshTokenConfigPath, token);
  if (!HostConfigToJsonFile(*config, config_file)) {
    LOG(ERROR) << "Failed to write updated config file.";
  }
}

}  // namespace remoting
