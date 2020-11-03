// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/daemon_process.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/scoped_sc_handle_win.h"
#include "remoting/host/branding.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/desktop_session_win.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/host_main.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/pairing_registry_delegate_win.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/host/switches.h"
#include "remoting/host/win/launch_process_with_token.h"
#include "remoting/host/win/security_descriptor.h"
#include "remoting/host/win/unprivileged_process_delegate.h"
#include "remoting/host/win/worker_process_launcher.h"

using base::win::ScopedHandle;
using base::TimeDelta;

namespace {

// Duplicates |key| and returns the value that can be sent over IPC.
IPC::PlatformFileForTransit GetRegistryKeyForTransit(
    const base::win::RegKey& key) {
  base::PlatformFile handle =
      reinterpret_cast<base::PlatformFile>(key.Handle());
  return IPC::GetPlatformFileForTransit(handle, false);
}

}  // namespace

namespace remoting {

class WtsTerminalMonitor;

// The command line parameters that should be copied from the service's command
// line to the host process.
const char kEnableVp9SwitchName[] = "enable-vp9";
const char kEnableH264SwitchName[] = "enable-h264";
const char* kCopiedSwitchNames[] = {switches::kV, switches::kVModule,
                                    kEnableVp9SwitchName,
                                    kEnableH264SwitchName};

class DaemonProcessWin : public DaemonProcess {
 public:
  DaemonProcessWin(scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
                   scoped_refptr<AutoThreadTaskRunner> io_task_runner,
                   base::OnceClosure stopped_callback);
  ~DaemonProcessWin() override;

  // WorkerProcessIpcDelegate implementation.
  void OnChannelConnected(int32_t peer_pid) override;
  void OnPermanentError(int exit_code) override;

  // DaemonProcess overrides.
  void SendToNetwork(IPC::Message* message) override;
  bool OnDesktopSessionAgentAttached(
      int terminal_id,
      int session_id,
      const IPC::ChannelHandle& desktop_pipe) override;

 protected:
  // DaemonProcess implementation.
  std::unique_ptr<DesktopSession> DoCreateDesktopSession(
      int terminal_id,
      const ScreenResolution& resolution,
      bool virtual_terminal) override;
  void DoCrashNetworkProcess(const base::Location& location) override;
  void LaunchNetworkProcess() override;

  // Changes the service start type to 'manual'.
  void DisableAutoStart();

  // Initializes the pairing registry on the host side by sending
  // ChromotingDaemonNetworkMsg_InitializePairingRegistry message.
  bool InitializePairingRegistry();

  // Opens the pairing registry keys.
  bool OpenPairingRegistry();

 private:
  // Mojo keeps the task runner passed to it alive forever, so an
  // AutoThreadTaskRunner should not be passed to it. Otherwise, the process may
  // never shut down cleanly.
  mojo::core::ScopedIPCSupport ipc_support_;

  std::unique_ptr<WorkerProcessLauncher> network_launcher_;

  // Handle of the network process.
  ScopedHandle network_process_;

  base::win::RegKey pairing_registry_privileged_key_;
  base::win::RegKey pairing_registry_unprivileged_key_;

  DISALLOW_COPY_AND_ASSIGN(DaemonProcessWin);
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

DaemonProcessWin::~DaemonProcessWin() {
}

void DaemonProcessWin::OnChannelConnected(int32_t peer_pid) {
  // Obtain the handle of the network process.
  network_process_.Set(OpenProcess(PROCESS_DUP_HANDLE, false, peer_pid));
  if (!network_process_.IsValid()) {
    CrashNetworkProcess(FROM_HERE);
    return;
  }

  if (!InitializePairingRegistry()) {
    CrashNetworkProcess(FROM_HERE);
    return;
  }

  DaemonProcess::OnChannelConnected(peer_pid);
}

void DaemonProcessWin::OnPermanentError(int exit_code) {
  DCHECK(kMinPermanentErrorExitCode <= exit_code &&
         exit_code <= kMaxPermanentErrorExitCode);

  // Both kInvalidHostIdExitCode and kInvalidOauthCredentialsExitCode are
  // errors then will never go away with the current config.
  // Disabling automatic service start until the host is re-enabled and config
  // updated.
  if (exit_code == kInvalidHostIdExitCode ||
      exit_code == kInvalidOauthCredentialsExitCode) {
    DisableAutoStart();
  }

  DaemonProcess::OnPermanentError(exit_code);
}

void DaemonProcessWin::SendToNetwork(IPC::Message* message) {
  if (network_launcher_) {
    network_launcher_->Send(message);
  } else {
    delete message;
  }
}

bool DaemonProcessWin::OnDesktopSessionAgentAttached(
    int terminal_id,
    int session_id,
    const IPC::ChannelHandle& desktop_pipe) {
  SendToNetwork(new ChromotingDaemonNetworkMsg_DesktopAttached(
      terminal_id, session_id, desktop_pipe));
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
                           kCopiedSwitchNames, base::size(kCopiedSwitchNames));

  std::unique_ptr<UnprivilegedProcessDelegate> delegate(
      new UnprivilegedProcessDelegate(io_task_runner(), std::move(target)));
  network_launcher_.reset(new WorkerProcessLauncher(std::move(delegate), this));
}

std::unique_ptr<DaemonProcess> DaemonProcess::Create(
    scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
    scoped_refptr<AutoThreadTaskRunner> io_task_runner,
    base::OnceClosure stopped_callback) {
  std::unique_ptr<DaemonProcessWin> daemon_process(new DaemonProcessWin(
      caller_task_runner, io_task_runner, std::move(stopped_callback)));
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
    if (!OpenPairingRegistry())
      return false;
  }

  // Duplicate handles to the network process.
  IPC::PlatformFileForTransit privileged_key = GetRegistryKeyForTransit(
      pairing_registry_privileged_key_);
  IPC::PlatformFileForTransit unprivileged_key = GetRegistryKeyForTransit(
      pairing_registry_unprivileged_key_);
  if (!(privileged_key.IsValid() && unprivileged_key.IsValid()))
    return false;

  // Initialize the pairing registry in the network process. This has to be done
  // before the host configuration is sent, otherwise the host will not use
  // the passed handles.
  SendToNetwork(new ChromotingDaemonNetworkMsg_InitializePairingRegistry(
      privileged_key, unprivileged_key));

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
  LONG result = root.CreateWithDisposition(
      HKEY_LOCAL_MACHINE, kPairingRegistryKeyName, &disposition,
      KEY_READ | KEY_CREATE_SUB_KEY);

  if (result != ERROR_SUCCESS) {
    ::SetLastError(result);
    PLOG(ERROR) << "Failed to open or create HKLM\\" << kPairingRegistryKeyName;
    return false;
  }

  if (disposition == REG_CREATED_NEW_KEY)
    LOG(WARNING) << "Created pairing registry root key which was absent.";

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

  if (disposition == REG_CREATED_NEW_KEY)
    LOG(WARNING) << "Created pairing registry client key which was absent.";

  // Open the pairing registry secret key.
  base::win::RegKey privileged;
  result = privileged.Open(
      root.Handle(), kPairingRegistrySecretsKeyName, KEY_READ | KEY_WRITE);

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
    result = ::RegCreateKeyEx(
        root.Handle(), kPairingRegistrySecretsKeyName, 0, nullptr, 0,
        KEY_READ | KEY_WRITE, &security_attributes, &key, &disposition);
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

}  // namespace remoting
