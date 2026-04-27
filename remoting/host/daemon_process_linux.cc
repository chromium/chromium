// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/daemon_process.h"

#include <signal.h>
#include <stdint.h>
#include <unistd.h>

#include <cstdlib>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/posix/safe_strerror.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
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
#include "remoting/base/branding.h"
#include "remoting/base/crash/crash_reporting_breakpad.h"
#include "remoting/base/logging.h"
#include "remoting/base/passwd_utils.h"
#include "remoting/base/username.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/chromoting_host_services_server.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_main.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/linux/desktop_session_factory_linux.h"
#include "remoting/host/linux/linux_process_launcher_delegate.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"
#include "remoting/host/mojom/remoting_host.mojom.h"
#include "remoting/host/pairing_registry_delegate_linux.h"
#include "remoting/host/usage_stats_consent.h"
#include "remoting/host/worker_process_launcher.h"

namespace remoting {

class DaemonProcessLinux : public DaemonProcess {
 public:
  DaemonProcessLinux(scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
                     scoped_refptr<AutoThreadTaskRunner> io_task_runner,
                     StoppedCallback stopped_callback);

  DaemonProcessLinux(const DaemonProcessLinux&) = delete;
  DaemonProcessLinux& operator=(const DaemonProcessLinux&) = delete;

  ~DaemonProcessLinux() override;

  // WorkerProcessIpcDelegate implementation.
  void OnChannelConnected(int32_t peer_pid) override;
  void OnWorkerProcessStopped() override;

  // DaemonProcess overrides.
  bool OnDesktopSessionAgentAttached(
      int terminal_id,
      mojo::ScopedMessagePipeHandle desktop_pipe) override;

  // mojom::ChromotingHostServices implementation.
  void BindSessionServices(
      mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver)
      override;

  void StartDesktopSessionFactory();

  void Cleanup(base::OnceClosure callback) override;

 private:
  // DaemonProcess implementation.
  std::unique_ptr<DesktopSession> DoCreateDesktopSession(
      int terminal_id,
      const mojom::DesktopSessionOptions& options) override;
  void DoCrashNetworkProcess(const base::Location& location) override;
  void LaunchNetworkProcess() override;
  void SendHostConfigToNetworkProcess(
      const std::string& serialized_config) override;
  void SendTerminalDisconnected(int terminal_id) override;

  void OnStartDesktopSessionFactoryResult(
      base::expected<void, Loggable> result);

  // Mojo keeps the task runner passed to it alive forever, so an
  // AutoThreadTaskRunner should not be passed to it. Otherwise, the process may
  // never shut down cleanly.
  mojo::core::ScopedIPCSupport ipc_support_;

  std::unique_ptr<WorkerProcessLauncher> network_launcher_;

  DesktopSessionFactoryLinux desktop_session_factory_;

  mojo::AssociatedRemote<mojom::DesktopSessionConnectionEvents>
      desktop_session_connection_events_;
  mojo::AssociatedRemote<mojom::RemotingHostControl> remoting_host_control_;
};

DaemonProcessLinux::DaemonProcessLinux(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    StoppedCallback stopped_callback)
    : DaemonProcess(caller_task_runner,
                    io_task_runner,
                    std::move(stopped_callback)),
      ipc_support_(io_task_runner->task_runner(),
                   mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST),
      desktop_session_factory_(io_task_runner) {}

DaemonProcessLinux::~DaemonProcessLinux() = default;

void DaemonProcessLinux::OnChannelConnected(int32_t peer_pid) {
  // Typically the Daemon process is responsible for disconnecting the remote
  // however in cases where the network process crashes, we want to ensure that
  // |remoting_host_control_| is reset so it can be reused after the network
  // process is relaunched.
  remoting_host_control_.reset();
  network_launcher_->GetRemoteAssociatedInterface(
      remoting_host_control_.BindNewEndpointAndPassReceiver());
  desktop_session_connection_events_.reset();
  network_launcher_->GetRemoteAssociatedInterface(
      desktop_session_connection_events_.BindNewEndpointAndPassReceiver());

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
    mojo::ScopedMessagePipeHandle desktop_pipe) {
  if (desktop_session_connection_events_) {
    desktop_session_connection_events_->OnDesktopSessionAgentAttached(
        terminal_id, std::move(desktop_pipe));
  }

  return true;
}

void DaemonProcessLinux::StartDesktopSessionFactory() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  desktop_session_factory_.Start(
      base::BindOnce(&DaemonProcessLinux::OnStartDesktopSessionFactoryResult,
                     base::Unretained(this)));
}

std::unique_ptr<DesktopSession> DaemonProcessLinux::DoCreateDesktopSession(
    int terminal_id,
    const mojom::DesktopSessionOptions& options) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  return desktop_session_factory_.CreateDesktopSession(terminal_id, this,
                                                       options);
}

void DaemonProcessLinux::DoCrashNetworkProcess(const base::Location& location) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  network_launcher_->Crash(location);
}

void DaemonProcessLinux::LaunchNetworkProcess() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  // TODO: crbug.com/475611769 - See if we need a dedicated desktop process
  // binary.
  base::FilePath this_exe;
  if (!base::PathService::Get(base::BasePathKey::FILE_EXE, &this_exe)) {
    LOG(ERROR) << "Failed to get the current executable path.";
    Stop(kInitializationFailed);
    return;
  }

  auto user_info = GetPasswdUserInfo(GetNetworkProcessUsername());
  if (!user_info.has_value()) {
    LOG(ERROR) << user_info.error();
    Stop(kInitializationFailed);
    return;
  }

  base::CommandLine command_line(this_exe);
  command_line.AppendSwitchASCII(kProcessTypeSwitchName, kProcessTypeNetwork);

  LinuxWorkerProcessLauncherDelegate::LaunchOptions options(command_line);
  options.new_session = true;
  options.uid = user_info->uid;
  options.gid = user_info->gid;
  // The home directory of the network user is /nonexistent, so we just change
  // the working directory to /tmp instead.
  base::FilePath temp_dir;
  if (!base::PathService::Get(base::DIR_TEMP, &temp_dir)) {
    LOG(ERROR) << "Failed to get the temporary directory path.";
    Stop(kInitializationFailed);
    return;
  }
  options.working_dir = temp_dir;
  options.environment_variables = {
      {"LOGNAME", GetNetworkProcessUsername().data()},
      {"USER", GetNetworkProcessUsername().data()},
  };
  network_launcher_ = std::make_unique<WorkerProcessLauncher>(
      std::make_unique<LinuxWorkerProcessLauncherDelegate>(std::move(options),
                                                           io_task_runner()),
      this);
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

void DaemonProcessLinux::OnStartDesktopSessionFactoryResult(
    base::expected<void, Loggable> result) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  if (!result.has_value()) {
    LOG(ERROR) << result.error();
    Stop(kInitializationFailed);
  }
}

void DaemonProcessLinux::Cleanup(base::OnceClosure callback) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  desktop_session_factory_.TerminateAllSessions(base::BindOnce(
      [](base::OnceClosure callback, base::expected<void, Loggable> result) {
        if (!result.has_value()) {
          LOG(ERROR) << result.error();
        } else {
          HOST_LOG << "All desktop sessions have been terminated.";
        }
        std::move(callback).Run();
      },
      std::move(callback)));
}

std::unique_ptr<DaemonProcess> DaemonProcess::Create(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    StoppedCallback stopped_callback) {
  auto daemon_process = std::make_unique<DaemonProcessLinux>(
      caller_task_runner, io_task_runner, std::move(stopped_callback));

  if (!PairingRegistryDelegateLinux::SetupMultiProcessPairingRegistry()) {
    return nullptr;
  }

  daemon_process->StartDesktopSessionFactory();

  // Finishes configuring the Daemon process and launches the network process.
  daemon_process->Initialize();

  return std::move(daemon_process);
}

void DaemonProcessLinux::BindSessionServices(
    mojo::PendingReceiver<mojom::ChromotingSessionServices> receiver) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  if (!desktop_session_connection_events_.is_bound()) {
    LOG(ERROR) << "Binding rejected. Network process is not ready.";
    return;
  }

  uid_t uid = host_services_receivers().current_context()->credentials.uid;
  DesktopSession* session = desktop_session_factory_.GetSessionByUid(uid);
  if (session) {
    desktop_session_connection_events_->OnSessionServicesClientConnected(
        session->id(), std::move(receiver));
  } else {
    LOG(WARNING) << "No desktop session found for UID " << uid;
  }
}

}  // namespace remoting
