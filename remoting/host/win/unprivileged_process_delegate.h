// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_UNPRIVILEGED_PROCESS_DELEGATE_H_
#define REMOTING_HOST_WIN_UNPRIVILEGED_PROCESS_DELEGATE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/launch.h"
#include "base/sequence_checker.h"
#include "base/win/scoped_handle.h"
#include "base/win/sid.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/win/security_descriptor.h"
#include "remoting/host/win/windows_process_delegate.h"

namespace base {
class SingleThreadTaskRunner;
namespace win {
class AccessToken;
}  // namespace win
}  // namespace base

namespace IPC {
class ChannelProxy;
}  // namespace IPC

namespace remoting {

// Implements logic for launching and monitoring a worker process under a less
// privileged user account.
class UnprivilegedProcessDelegate : public IPC::Listener,
                                    public WindowsProcessDelegate {
 public:
  enum class IntegrityLevel {
    kLow,
    kUntrusted,
  };

  UnprivilegedProcessDelegate(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      std::unique_ptr<base::CommandLine> target_command,
      IntegrityLevel integrity_level);

  UnprivilegedProcessDelegate(const UnprivilegedProcessDelegate&) = delete;
  UnprivilegedProcessDelegate& operator=(const UnprivilegedProcessDelegate&) =
      delete;

  ~UnprivilegedProcessDelegate() override;

  // Derives the deterministic AppContainer SID for `profile_name`, creates a
  // restricted Untrusted-IL `LocalService` LowBox token for it without creating
  // an on-disk/registry AppContainer profile, and returns the open token
  // handle. Holding this handle open reserves and pins a Session 0 LowBox
  // table slot early after boot, working around a Windows 11 24H2/25H2 kernel
  // LowBox reference-counting leak where transient Session 0 AppContainers
  // eventually exhaust all 65,535 (`0xFFFE`) per-session slots and cause
  // subsequent AppContainer launches to fail with `ERROR_NO_SYSTEM_RESOURCES`
  // (see crbug.com/582900491). Returns an invalid handle on failure.
  static base::win::ScopedHandle PrewarmAppContainer(
      const std::wstring& profile_name);

  // If non-empty, the worker process will be launched inside an AppContainer
  // with the specified profile name.
  void UseAppContainer(const std::wstring& profile_name);

  // WorkerProcessLauncher::Delegate implementation.
  void LaunchProcess(WorkerProcessLauncher* event_handler) override;
  void GetRemoteAssociatedInterface(
      mojo::GenericPendingAssociatedReceiver receiver) override;
  void CloseChannel() override;
  void CrashProcess(const base::Location& location) override;
  void KillProcess() override;

 protected:
  // TODO(crbug.com/582900491): Revisit how process launching is structured for
  // testing (e.g., narrow this virtual seam or return `base::expected<...,
  // DWORD>` instead of `last_error_out`).
  virtual bool DoLaunchProcessWithToken(
      const base::FilePath& binary,
      const base::CommandLine::StringType& command_line,
      HANDLE user_token,
      SECURITY_ATTRIBUTES* process_attributes,
      SECURITY_ATTRIBUTES* thread_attributes,
      const base::HandlesToInheritVector& handles_to_inherit,
      SECURITY_CAPABILITIES* security_capabilities,
      DWORD creation_flags,
      const wchar_t* desktop_name,
      base::win::ScopedHandle* process_out,
      base::win::ScopedHandle* thread_out,
      DWORD* last_error_out);
  virtual void ReportProcessLaunched(base::win::ScopedHandle worker_process);

 private:
  friend class UnprivilegedProcessDelegateTest;

  struct AppContainer {
    AppContainer();
    AppContainer(AppContainer&&);
    AppContainer& operator=(AppContainer&&);
    ~AppContainer();

    SECURITY_CAPABILITIES GetSecurityCapabilities();

    ScopedSid package_sid;
    std::vector<base::win::Sid> capability_sids;
    std::vector<SID_AND_ATTRIBUTES> capabilities;
    std::wstring profile_name;
  };

  static std::optional<AppContainer> CreateAppContainer(
      const std::wstring& profile_name);

  // Derives the deterministic AppContainer SID for `profile_name` without
  // creating or modifying an on-disk/registry AppContainer profile, creates a
  // LowBox token from `base_token`, and returns the open token handle so the
  // caller can keep a LowBox slot allocated and referenced in `base_token`'s
  // WTS session (kernel LowBox entries are keyed by `AppContainerSid`; see
  // crbug.com/582900491). Separated from `PrewarmAppContainer()` so unit tests
  // can supply an unprivileged caller token instead of creating a
  // `LocalService` token (which requires `SYSTEM`).
  static base::win::ScopedHandle PrewarmAppContainerForToken(
      const std::wstring& profile_name,
      const base::win::AccessToken& base_token);

  // Launches the worker process via `DoLaunchProcessWithToken()`. If
  // `app_container_` is active and `CreateProcessAsUserW()` fails specifically
  // with `ERROR_NO_SYSTEM_RESOURCES` (`0x5AA`), resets `app_container_` and
  // retries once without `SECURITY_CAPABILITIES` (still running under the
  // restricted `LocalService` token and job object). This allows worker
  // launches to succeed on machines where all 65,535 Session 0 LowBox slots
  // were already exhausted before the host started or upgraded in place (see
  // crbug.com/582900491).
  bool LaunchWorkerProcessWithFallback(
      const base::CommandLine& command_line,
      HANDLE user_token,
      SECURITY_ATTRIBUTES* process_attributes,
      SECURITY_ATTRIBUTES* thread_attributes,
      const base::HandlesToInheritVector& handles_to_inherit,
      SECURITY_CAPABILITIES* security_capabilities,
      base::win::ScopedHandle* worker_process_out,
      base::win::ScopedHandle* worker_thread_out);

  // IPC::Listener implementation.
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  void ReportFatalError();

  // The task runner serving job object notifications.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Command line of the launched process.
  std::unique_ptr<base::CommandLine> target_command_;

  // The server end of the IPC channel used to communicate to the worker
  // process.
  std::unique_ptr<IPC::ChannelProxy> channel_;

  mojo::AssociatedRemote<mojom::WorkerProcessControl> worker_process_control_;

  IntegrityLevel integrity_level_;
  std::wstring app_container_profile_name_;
  std::optional<AppContainer> app_container_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_UNPRIVILEGED_PROCESS_DELEGATE_H_
