// Copyright 2012 The Chromium Authors
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
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/breakpad.h"
#include "remoting/base/logging.h"
#include "remoting/base/scoped_sc_handle_win.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/branding.h"
#include "remoting/host/chromoting_host_services_server.h"
#include "remoting/host/crash/minidump_handler.h"
#include "remoting/host/desktop_session_win.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_main.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"
#include "remoting/host/mojom/remoting_host.mojom.h"
#include "remoting/host/pairing_registry_delegate_win.h"
#include "remoting/host/usage_stats_consent.h"
#include "remoting/host/win/etw_trace_consumer.h"
#include "remoting/host/win/host_event_file_logger.h"
#include "remoting/host/win/host_event_windows_event_logger.h"
#include "remoting/host/win/launch_process_with_token.h"
#include "remoting/host/win/security_descriptor.h"
#include "remoting/host/win/unprivileged_process_delegate.h"
#include "remoting/host/win/worker_process_launcher.h"

using base::win::ScopedHandle;

namespace {

constexpr char kEtwTracingThreadName[] = "ETW Trace Consumer";

// Duplicates |key| and returns a value that can be sent over IPC.
base::win::ScopedHandle DuplicateRegistryKeyHandle(
    const base::win::RegKey& key) {
  HANDLE duplicate_handle = INVALID_HANDLE_VALUE;
  BOOL result = ::DuplicateHandle(::GetCurrentProcess(),
                                  reinterpret_cast<HANDLE>(key.Handle()),
                                  ::GetCurrentProcess(), &duplicate_handle, 0,
                                  FALSE, DUPLICATE_SAME_ACCESS);
  if (!result || duplicate_handle == INVALID_HANDLE_VALUE) {
    return base::win::ScopedHandle();
  }
  return base::win::ScopedHandle(duplicate_handle);
}

#if defined(OFFICIAL_BUILD)
constexpr wchar_t kLoggingRegistryKeyName[] =
    L"SOFTWARE\\Google\\Chrome Remote Desktop\\logging";
#else
constexpr wchar_t kLoggingRegistryKeyName[] = L"SOFTWARE\\Chromoting\\logging";
#endif

constexpr wchar_t kLogToFileRegistryValue[] = L"LogToFile";
constexpr wchar_t kLogToEventLogRegistryValue[] = L"LogToEventLog";

const char* const kCopiedSwitchNames[] = {switches::kV, switches::kVModule};

}  // namespace

namespace remoting {

class WtsTerminalMonitor;

class DaemonProcessWin : public DaemonProcess {
 public:
  DaemonProcessWin(scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
                   scoped_refptr<AutoThreadTaskRunner> io_task_runner,
                   base::OnceClosure stopped_callback);

  DaemonProcessWin(const DaemonProcessWin&) = delete;
  DaemonProcessWin& operator=(const DaemonProcessWin&) = delete;

  ~DaemonProcessWin() override;

  // WorkerProcessIpcDelegate implementation.
  void OnChannelConnected(int32_t peer_pid) override;
  void OnPermanentError(int exit_code) override;
  void OnWorkerProcessStopped() override;

  // DaemonProcess overrides.
  bool OnDesktopSessionAgentAttached(
      int terminal_id,
      int session_id,
      mojo::ScopedMessagePipeHandle desktop_pipe) override;

  // If event logging has been configured, creates an ETW trace consumer which
  // listens for logged events from our host processes.  Tracing stops when
  // |etw_trace_consumer_| is destroyed.  Logging destinations are configured
  // via the registry.
  void ConfigureHostLogging();

  // If the user has consented to crash reporting, this method will start a
  // BreakpadServer instance to handle crashes from the network process.
  void ConfigureCrashReporting();

 protected:
  // DaemonProcess implementation.
  std::unique_ptr<DesktopSession> DoCreateDesktopSession(
      int terminal_id,
      const ScreenResolution& resolution,
      bool virtual_terminal) override;
  void DoCrashNetworkProcess(const base::Location& location) override;
  void LaunchNetworkProcess() override;
  void SendHostConfigToNetworkProcess(
      const std::string& serialized_config) override;
  void SendTerminalDisconnected(int terminal_id) override;
  void StartChromotingHostServices() override;

  // Changes the service start type to 'manual'.
  void DisableAutoStart();

  // Initializes the pairing registry on the host side.
  bool InitializePairingRegistry();

  // Opens the pairing registry keys.
  bool OpenPairingRegistry();

 private:
  void BindChromotingHostServices(
      mojo::PendingReceiver<mojom::ChromotingHostServices> receiver,
      base::ProcessId peer_pid);

  // Mojo keeps the task runner passed to it alive forever, so an
  // AutoThreadTaskRunner should not be passed to it. Otherwise, the process may
  // never shut down cleanly.
  mojo::core::ScopedIPCSupport ipc_support_;

  std::unique_ptr<WorkerProcessLauncher> network_launcher_;

  // Handle of the network process.
  ScopedHandle network_process_;

  base::win::RegKey pairing_registry_privileged_key_;
  base::win::RegKey pairing_registry_unprivileged_key_;

  std::unique_ptr<EtwTraceConsumer> etw_trace_consumer_;

  std::unique_ptr<ChromotingHostServicesServer> ipc_server_;

  base::SequenceBound<MinidumpHandler> minidump_handler_;

  mojo::AssociatedRemote<mojom::DesktopSessionConnectionEvents>
      desktop_session_connection_events_;
  mojo::AssociatedRemote<mojom::RemotingHostControl> remoting_host_control_;
};

DaemonProcessWin::DaemonProcessWin(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    base::OnceClosure stopped_callback)
    : DaemonProcess(caller_task_runner,
                    io_task_runner,
                    std::move(stopped_callback)),
      ipc_support_(io_task_runner->task_runner(),
                   mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST) {}

DaemonProcessWin::~DaemonProcessWin() = default;

void DaemonProcessWin::OnChannelConnected(int32_t peer_pid) {
  // Obtain the handle of the network process.
  network_process_.Set(OpenProcess(PROCESS_DUP_HANDLE, false, peer_pid));
  if (!network_process_.IsValid()) {
    CrashNetworkProcess(FROM_HERE);
    return;
  }

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

  if (!InitializePairingRegistry()) {
    CrashNetworkProcess(FROM_HERE);
    return;
  }

  DaemonProcess::OnChannelConnected(peer_pid);
}

void DaemonProcessWin::OnPermanentError(int exit_code) {
  DCHECK(kMinPermanentErrorExitCode <= exit_code &&
         exit_code <= kMaxPermanentErrorExitCode);

  // Both kInvalidHostIdExitCode and kInvalidOAuthCredentialsExitCode are
  // errors that will never go away with the current config.
  // Disabling automatic service start until the host is re-enabled and config
  // updated.
  if (exit_code == kInvalidHostIdExitCode ||
      exit_code == kInvalidOAuthCredentialsExitCode) {
    DisableAutoStart();
  }

  DaemonProcess::OnPermanentError(exit_code);
}

void DaemonProcessWin::OnWorkerProcessStopped() {
  // Reset our IPC remote so it's ready to re-init if the network process is
  // re-launched.
  remoting_host_control_.reset();
  desktop_session_connection_events_.reset();

  DaemonProcess::OnWorkerProcessStopped();
}

bool DaemonProcessWin::OnDesktopSessionAgentAttached(
    int terminal_id,
    int session_id,
    mojo::ScopedMessagePipeHandle desktop_pipe) {
  if (desktop_session_connection_events_) {
    desktop_session_connection_events_->OnDesktopSessionAgentAttached(
        terminal_id, session_id, std::move(desktop_pipe));
  }

  return true;
}

std::unique_ptr<DesktopSession> DaemonProcessWin::DoCreateDesktopSession(
    int terminal_id,
    const ScreenResolution& resolution,
    bool virtual_terminal) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  if (virtual_terminal) {
    return DesktopSessionWin::CreateForVirtualTerminal(
        caller_task_runner(), io_task_runner(), this, terminal_id, resolution);
  } else {
    return DesktopSessionWin::CreateForConsole(
        caller_task_runner(), io_task_runner(), this, terminal_id, resolution);
  }
}

void DaemonProcessWin::DoCrashNetworkProcess(const base::Location& location) {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());

  network_launcher_->Crash(location);
}

void DaemonProcessWin::LaunchNetworkProcess() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  DCHECK(!network_launcher_);

  // Construct the host binary name.
  base::FilePath host_binary;
  if (!GetInstalledBinaryPath(kHostBinaryName, &host_binary)) {
    Stop();
    return;
  }

  std::unique_ptr<base::CommandLine> target(new base::CommandLine(host_binary));
  target->AppendSwitchASCII(kProcessTypeSwitchName, kProcessTypeHost);
  target->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                           kCopiedSwitchNames);

  std::unique_ptr<UnprivilegedProcessDelegate> delegate(
      new UnprivilegedProcessDelegate(io_task_runner(), std::move(target)));
  network_launcher_ =
      std::make_unique<WorkerProcessLauncher>(std::move(delegate), this);
}

void DaemonProcessWin::SendHostConfigToNetworkProcess(
    const std::string& serialized_config) {
  if (!remoting_host_control_) {
    return;
  }

  LOG_IF(ERROR, !remoting_host_control_.is_connected())
      << "IPC channel not connected. HostConfig message will be dropped.";

  std::optional<base::Value::Dict> config(
      HostConfigFromJson(serialized_config));
  if (!config.has_value()) {
    LOG(ERROR) << "Invalid host config, shutting down.";
    OnPermanentError(kInvalidHostConfigurationExitCode);
    return;
  }

  remoting_host_control_->ApplyHostConfig(std::move(config.value()));
}

void DaemonProcessWin::SendTerminalDisconnected(int terminal_id) {
  if (desktop_session_connection_events_) {
    desktop_session_connection_events_->OnTerminalDisconnected(terminal_id);
  }
}

void DaemonProcessWin::StartChromotingHostServices() {
  DCHECK(caller_task_runner()->BelongsToCurrentThread());
  DCHECK(!ipc_server_);

  ipc_server_ = std::make_unique<ChromotingHostServicesServer>(
      base::BindRepeating(&DaemonProcessWin::BindChromotingHostServices,
                          base::Unretained(this)));
  ipc_server_->StartServer();
  HOST_LOG << "ChromotingHostServices IPC server has been started.";
}

std::unique_ptr<DaemonProcess> DaemonProcess::Create(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    base::OnceClosure stopped_callback) {
  auto daemon_process = std::make_unique<DaemonProcessWin>(
      caller_task_runner, io_task_runner, std::move(stopped_callback));

  // Configure host logging first so we can capture subsequent events.
  daemon_process->ConfigureHostLogging();

  // Initialize crash reporting before the network process is launched.
  daemon_process->ConfigureCrashReporting();

  // Finishes configuring the Daemon process and launches the network process.
  daemon_process->Initialize();

  return std::move(daemon_process);
}

void DaemonProcessWin::DisableAutoStart() {
  ScopedScHandle scmanager(
      OpenSCManager(nullptr, SERVICES_ACTIVE_DATABASE,
                    SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE));
  if (!scmanager.IsValid()) {
    PLOG(INFO) << "Failed to connect to the service control manager";
    return;
  }

  DWORD desired_access = SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS;
  ScopedScHandle service(
      OpenService(scmanager.Get(), kWindowsServiceName, desired_access));
  if (!service.IsValid()) {
    PLOG(INFO) << "Failed to open to the '" << kWindowsServiceName
               << "' service";
    return;
  }

  // Change the service start type to 'manual'. All |nullptr| parameters below
  // mean that there is no change to the corresponding service parameter.
  if (!ChangeServiceConfig(service.Get(),
                           SERVICE_NO_CHANGE,
                           SERVICE_DEMAND_START,
                           SERVICE_NO_CHANGE,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr)) {
    PLOG(INFO) << "Failed to change the '" << kWindowsServiceName
               << "'service start type to 'manual'";
  }
}

bool DaemonProcessWin::InitializePairingRegistry() {
  if (!pairing_registry_privileged_key_.Valid()) {
    if (!OpenPairingRegistry()) {
      return false;
    }
  }

  // Initialize the pairing registry in the network process. This has to be done
  // before the host configuration is sent, otherwise the host will not use
  // the passed handles.

  // Duplicate handles for the network process.
  base::win::ScopedHandle privileged_key =
      DuplicateRegistryKeyHandle(pairing_registry_privileged_key_);
  base::win::ScopedHandle unprivileged_key =
      DuplicateRegistryKeyHandle(pairing_registry_unprivileged_key_);
  if (!(privileged_key.IsValid() && unprivileged_key.IsValid())) {
    return false;
  }

  if (!remoting_host_control_) {
    return false;
  }

  remoting_host_control_->InitializePairingRegistry(
      mojo::PlatformHandle(std::move(privileged_key)),
      mojo::PlatformHandle(std::move(unprivileged_key)));

  return true;
}

// A chromoting top crasher revealed that the pairing registry keys sometimes
// cannot be opened. The speculation is that those keys are absent for some
// reason. To reduce the host crashes we create those keys here if they are
// absent. See crbug.com/379360 for details.
bool DaemonProcessWin::OpenPairingRegistry() {
  DCHECK(!pairing_registry_privileged_key_.Valid());
  DCHECK(!pairing_registry_unprivileged_key_.Valid());

  // Open the root of the pairing registry. Create if absent.
  base::win::RegKey root;
  DWORD disposition;
  LONG result =
      root.CreateWithDisposition(HKEY_LOCAL_MACHINE, kPairingRegistryKeyName,
                                 &disposition, KEY_READ | KEY_CREATE_SUB_KEY);

  if (result != ERROR_SUCCESS) {
    ::SetLastError(result);
    PLOG(ERROR) << "Failed to open or create HKLM\\" << kPairingRegistryKeyName;
    return false;
  }

  if (disposition == REG_CREATED_NEW_KEY) {
    LOG(WARNING) << "Created pairing registry root key which was absent.";
  }

  // Open the pairing registry clients key. Create if absent.
  base::win::RegKey unprivileged;
  result = unprivileged.CreateWithDisposition(
      root.Handle(), kPairingRegistryClientsKeyName, &disposition,
      KEY_READ | KEY_WRITE);

  if (result != ERROR_SUCCESS) {
    ::SetLastError(result);
    PLOG(ERROR) << "Failed to open or create HKLM\\" << kPairingRegistryKeyName
                << "\\" << kPairingRegistryClientsKeyName;
    return false;
  }

  if (disposition == REG_CREATED_NEW_KEY) {
    LOG(WARNING) << "Created pairing registry client key which was absent.";
  }

  // Open the pairing registry secret key.
  base::win::RegKey privileged;
  result = privileged.Open(root.Handle(), kPairingRegistrySecretsKeyName,
                           KEY_READ | KEY_WRITE);

  if (result == ERROR_FILE_NOT_FOUND) {
    LOG(WARNING) << "Pairing registry privileged key absent, creating.";

    // Create a security descriptor that gives full access to local system and
    // administrators and denies access by anyone else.
    std::string security_descriptor = "O:BAG:BAD:(A;;GA;;;BA)(A;;GA;;;SY)";

    ScopedSd sd = ConvertSddlToSd(security_descriptor);
    if (!sd) {
      PLOG(ERROR) << "Failed to create a security descriptor for the pairing"
                  << "registry privileged key.";
      return false;
    }

    SECURITY_ATTRIBUTES security_attributes = {0};
    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.lpSecurityDescriptor = sd.get();
    security_attributes.bInheritHandle = FALSE;

    HKEY key = nullptr;
    result = ::RegCreateKeyEx(root.Handle(), kPairingRegistrySecretsKeyName, 0,
                              nullptr, 0, KEY_READ | KEY_WRITE,
                              &security_attributes, &key, &disposition);
    privileged.Set(key);
  }

  if (result != ERROR_SUCCESS) {
    ::SetLastError(result);
    PLOG(ERROR) << "Failed to open or create HKLM\\" << kPairingRegistryKeyName
                << "\\" << kPairingRegistrySecretsKeyName;
    return false;
  }

  pairing_registry_privileged_key_.Set(privileged.Take());
  pairing_registry_unprivileged_key_.Set(unprivileged.Take());
  return true;
}

void DaemonProcessWin::BindChromotingHostServices(
    mojo::PendingReceiver<mojom::ChromotingHostServices> receiver,
    base::ProcessId peer_pid) {
  if (!remoting_host_control_.is_bound()) {
    LOG(ERROR) << "Binding rejected. Network process is not ready.";
    return;
  }
  remoting_host_control_->BindChromotingHostServices(std::move(receiver),
                                                     peer_pid);
}

void DaemonProcessWin::ConfigureCrashReporting() {
  if (IsUsageStatsAllowed()) {
    InitializeOopCrashServer();

    minidump_handler_.emplace(base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
  }
}

void DaemonProcessWin::ConfigureHostLogging() {
  DCHECK(!etw_trace_consumer_);

  base::win::RegKey logging_reg_key;
  LONG result = logging_reg_key.Open(HKEY_LOCAL_MACHINE,
                                     kLoggingRegistryKeyName, KEY_READ);
  if (result != ERROR_SUCCESS) {
    ::SetLastError(result);
    PLOG(ERROR) << "Failed to open HKLM\\" << kLoggingRegistryKeyName;
    return;
  }

  std::vector<std::unique_ptr<HostEventLogger>> loggers;

  // Check to see if file logging has been enabled.
  if (logging_reg_key.HasValue(kLogToFileRegistryValue)) {
    DWORD enabled = 0;
    result = logging_reg_key.ReadValueDW(kLogToFileRegistryValue, &enabled);
    if (result != ERROR_SUCCESS) {
      ::SetLastError(result);
      PLOG(ERROR) << "Failed to read HKLM\\" << kLoggingRegistryKeyName << "\\"
                  << kLogToFileRegistryValue;
    } else if (enabled) {
      auto file_logger = HostEventFileLogger::Create();
      if (file_logger) {
        loggers.push_back(std::move(file_logger));
      }
    }
  }

  // Check to see if Windows event logging has been enabled.
  if (logging_reg_key.HasValue(kLogToEventLogRegistryValue)) {
    DWORD enabled = 0;
    result = logging_reg_key.ReadValueDW(kLogToEventLogRegistryValue, &enabled);
    if (result != ERROR_SUCCESS) {
      ::SetLastError(result);
      PLOG(ERROR) << "Failed to read HKLM\\" << kLoggingRegistryKeyName << "\\"
                  << kLogToEventLogRegistryValue;
    } else if (enabled) {
      auto event_logger = HostEventWindowsEventLogger::Create();
      if (event_logger) {
        loggers.push_back(std::move(event_logger));
      }
    }
  }

  if (loggers.empty()) {
    VLOG(1) << "No host event loggers have been configured.";
    return;
  }

  etw_trace_consumer_ = EtwTraceConsumer::Create(
      AutoThread::CreateWithType(kEtwTracingThreadName, caller_task_runner(),
                                 base::MessagePumpType::IO),
      std::move(loggers));
}

}  // namespace remoting
