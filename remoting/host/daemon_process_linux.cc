// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/daemon_process.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/values.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/crash/crash_reporting_breakpad.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/branding.h"
#include "remoting/host/chromoting_host_services_server.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_main.h"
#include "remoting/host/linux/remote_display_session_manager.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"
#include "remoting/host/mojom/remoting_host.mojom.h"
#include "remoting/host/usage_stats_consent.h"

namespace remoting {

class DaemonProcessLinux : public DaemonProcess,
                           public RemoteDisplaySessionManager::Delegate {
 public:
  DaemonProcessLinux(scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
                     scoped_refptr<AutoThreadTaskRunner> io_task_runner,
                     base::OnceClosure stopped_callback);

  DaemonProcessLinux(const DaemonProcessLinux&) = delete;
  DaemonProcessLinux& operator=(const DaemonProcessLinux&) = delete;

  ~DaemonProcessLinux() override;

  // WorkerProcessIpcDelegate implementation.
  void OnChannelConnected(int32_t peer_pid) override;
  void OnWorkerProcessStopped() override;

  // DaemonProcess overrides.
  bool OnDesktopSessionAgentAttached(
      int terminal_id,
      int session_id,
      mojo::ScopedMessagePipeHandle desktop_pipe) override;

  void StartRemoteDisplaySessionManager();

 private:
  // DaemonProcess implementation.
  std::unique_ptr<DesktopSession> DoCreateDesktopSession(
      int terminal_id,
      const ScreenResolution& resolution,
      bool is_curtained) override;
  void DoCrashNetworkProcess(const base::Location& location) override;
  void LaunchNetworkProcess() override;
  void SendHostConfigToNetworkProcess(
      const std::string& serialized_config) override;
  void SendTerminalDisconnected(int terminal_id) override;
  void StartChromotingHostServices() override;

  void OnStartRemoteDisplaySessionManagerResult(
      base::expected<void, Loggable> result);

  // RemoteDisplaySessionManager::Delegate:
  void OnRemoteDisplaySessionChanged(
      std::string_view display_name,
      const RemoteDisplaySessionManager::RemoteDisplayInfo& info) override;
  void OnRemoteDisplayTerminated(std::string_view display_name) override;

  void BindChromotingHostServices(
      mojo::PendingReceiver<mojom::ChromotingHostServices> receiver,
      base::ProcessId peer_pid);

  // Mojo keeps the task runner passed to it alive forever, so an
  // AutoThreadTaskRunner should not be passed to it. Otherwise, the process may
  // never shut down cleanly.
  mojo::core::ScopedIPCSupport ipc_support_;

  std::unique_ptr<ChromotingHostServicesServer> ipc_server_;
  RemoteDisplaySessionManager remote_display_session_manager_;

  mojo::AssociatedRemote<mojom::DesktopSessionConnectionEvents>
      desktop_session_connection_events_;
  mojo::AssociatedRemote<mojom::RemotingHostControl> remoting_host_control_;
};

DaemonProcessLinux::DaemonProcessLinux(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    base::OnceClosure stopped_callback)
    : DaemonProcess(caller_task_runner,
                    io_task_runner,
                    std::move(stopped_callback)),
      ipc_support_(io_task_runner->task_runner(),
                   mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST) {}

DaemonProcessLinux::~DaemonProcessLinux() = default;

void DaemonProcessLinux::OnChannelConnected(int32_t peer_pid) {
  NOTIMPLEMENTED();

  DaemonProcess::OnChannelConnected(peer_pid);
}

void DaemonProcessLinux::OnWorkerProcessStopped() {
  // Reset our IPC remote so it's ready to re-init if the network process is
  // re-launched.
  remoting_host_control_.reset();
  desktop_session_connection_events_.reset();

  DaemonProcess::OnWorkerProcessStopped();
}

bool DaemonProcessLinux::OnDesktopSessionAgentAttached(
    int terminal_id,
    int session_id,
    mojo::ScopedMessagePipeHandle desktop_pipe) {
  if (desktop_session_connection_events_) {
    desktop_session_connection_events_->OnDesktopSessionAgentAttached(
        terminal_id, session_id, std::move(desktop_pipe));
  }

  return true;
}

void DaemonProcessLinux::StartRemoteDisplaySessionManager() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  remote_display_session_manager_.Start(
      this, base::BindOnce(
                &DaemonProcessLinux::OnStartRemoteDisplaySessionManagerResult,
                base::Unretained(this)));
}

std::unique_ptr<DesktopSession> DaemonProcessLinux::DoCreateDesktopSession(
    int terminal_id,
    const ScreenResolution& resolution,
    bool is_curtained) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  NOTIMPLEMENTED();
  return nullptr;
}

void DaemonProcessLinux::DoCrashNetworkProcess(const base::Location& location) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  NOTIMPLEMENTED();
}

void DaemonProcessLinux::LaunchNetworkProcess() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  NOTIMPLEMENTED();
}

void DaemonProcessLinux::SendHostConfigToNetworkProcess(
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

void DaemonProcessLinux::SendTerminalDisconnected(int terminal_id) {
  if (desktop_session_connection_events_) {
    desktop_session_connection_events_->OnTerminalDisconnected(terminal_id);
  }
}

void DaemonProcessLinux::StartChromotingHostServices() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  DCHECK(!ipc_server_);

  ipc_server_ = std::make_unique<ChromotingHostServicesServer>(
      base::BindRepeating(&DaemonProcessLinux::BindChromotingHostServices,
                          base::Unretained(this)));
  ipc_server_->StartServer();
  HOST_LOG << "ChromotingHostServices IPC server has been started.";
}

void DaemonProcessLinux::OnStartRemoteDisplaySessionManagerResult(
    base::expected<void, Loggable> result) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  if (!result.has_value()) {
    LOG(ERROR) << result.error();
    return;
  }
  remote_display_session_manager_.CreateRemoteDisplay(
      "test", base::BindOnce([](base::expected<void, Loggable> result) {
        if (!result.has_value()) {
          LOG(ERROR) << result.error();
        }
      }));
}

void DaemonProcessLinux::OnRemoteDisplaySessionChanged(
    std::string_view display_name,
    const RemoteDisplaySessionManager::RemoteDisplayInfo& info) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  LOG(INFO) << "Remote display session changed.";
  LOG(INFO) << "  display_name: " << display_name;
  LOG(INFO) << "  session_object_path: "
            << info.session_info->object_path.value();
  for (const auto& [key, value] : info.environment_variables) {
    LOG(INFO) << "  " << key << "=" << value;
  }
}

void DaemonProcessLinux::OnRemoteDisplayTerminated(
    std::string_view display_name) {
  LOG(INFO) << "Remote display terminated: " << display_name;
}

std::unique_ptr<DaemonProcess> DaemonProcess::Create(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    base::OnceClosure stopped_callback) {
  auto daemon_process = std::make_unique<DaemonProcessLinux>(
      caller_task_runner, io_task_runner, std::move(stopped_callback));

  daemon_process->StartRemoteDisplaySessionManager();

  // Finishes configuring the Daemon process and launches the network process.
  daemon_process->Initialize();

  return std::move(daemon_process);
}

void DaemonProcessLinux::BindChromotingHostServices(
    mojo::PendingReceiver<mojom::ChromotingHostServices> receiver,
    base::ProcessId peer_pid) {
  if (!remoting_host_control_.is_bound()) {
    LOG(ERROR) << "Binding rejected. Network process is not ready.";
    return;
  }
  remoting_host_control_->BindChromotingHostServices(std::move(receiver),
                                                     peer_pid);
}

}  // namespace remoting
