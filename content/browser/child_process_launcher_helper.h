// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_HELPER_H_
#define CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_HELPER_H_

#include <map>
#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/zygote/zygote_buildflags.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "ppapi/buildflags/buildflags.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "mojo/public/cpp/platform/named_platform_channel.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "sandbox/win/src/sandbox_types.h"
#else
#include "content/public/browser/posix_file_descriptor_info.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "sandbox/mac/seatbelt_exec.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_FUCHSIA)
#include "sandbox/policy/fuchsia/sandbox_policy_fuchsia.h"
#endif

#if BUILDFLAG(USE_ZYGOTE)
#include "content/public/common/zygote/zygote_handle.h"  // nogncheck
#endif

namespace base {
class CommandLine;

#if BUILDFLAG(IS_IOS)
class MachPortRendezvousServerIOS;
#endif
}

namespace content {

class ChildProcessLauncher;
class SandboxedProcessLauncherDelegate;
struct ChildProcessLauncherFileData;
struct ChildProcessTerminationInfo;
struct RenderProcessPriority;

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
class PosixFileDescriptorInfo;
#endif

namespace internal {

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
using FileMappedForLaunch = PosixFileDescriptorInfo;
#else
using FileMappedForLaunch = base::HandlesToInheritVector;
#endif

#if BUILDFLAG(IS_IOS)
class LaunchResult;

class ProcessStorageBase {
 public:
  virtual ~ProcessStorageBase() = default;
  virtual void ReleaseProcess() = 0;
};
#endif

// ChildProcessLauncherHelper is used by ChildProcessLauncher to start a
// process. Since ChildProcessLauncher can be deleted by its client at any time,
// this class is used to keep state as the process is started asynchronously.
// It also contains the platform specific pieces.
class ChildProcessLauncherHelper
    : public base::RefCountedThreadSafe<ChildProcessLauncherHelper> {
 public:
  // Abstraction around a process required to deal in a platform independent way
  // between Linux (which can use zygotes) and the other platforms.
  struct Process {
    Process();
    Process(Process&& other);
    ~Process();
    Process& operator=(Process&& other);

    base::Process process;

#if BUILDFLAG(USE_ZYGOTE)
    raw_ptr<ZygoteCommunication> zygote = nullptr;
#endif  // BUILDFLAG(USE_ZYGOTE)

#if BUILDFLAG(IS_FUCHSIA)
    // Store `sandbox_policy` within `Process` to ensure that the sandbox policy
    // isn't removed before the process is terminated.
    std::unique_ptr<sandbox::policy::SandboxPolicyFuchsia> sandbox_policy;
#endif
  };

  ChildProcessLauncherHelper(
      int child_process_id,
      std::unique_ptr<base::CommandLine> command_line,
      std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
      const base::WeakPtr<ChildProcessLauncher>& child_process_launcher,
      bool terminate_on_shutdown,
#if BUILDFLAG(IS_ANDROID)
      bool is_pre_warmup_required,
#endif
      mojo::OutgoingInvitation mojo_invitation,
      const mojo::ProcessErrorCallback& process_error_callback,
      std::unique_ptr<ChildProcessLauncherFileData> file_data,
      base::UnsafeSharedMemoryRegion histogram_memory_region,
      base::ReadOnlySharedMemoryRegion tracing_config_memory_region);

  // The methods below are defined in the order they are called.

  // Starts the flow of launching the process.
  void StartLaunchOnClientThread();

  // Platform specific.
  void BeforeLaunchOnClientThread();

#if !BUILDFLAG(IS_FUCHSIA)
  // Called to give implementors a chance at creating a server pipe. Platform-
  // specific. Returns |std::nullopt| if the helper should initialize
  // a regular PlatformChannel for communication instead.
  std::optional<mojo::NamedPlatformChannel>
  CreateNamedPlatformChannelOnLauncherThread();
#endif

  // Returns the list of files that should be mapped in the child process.
  // Platform specific.
  std::unique_ptr<FileMappedForLaunch> GetFilesToMap();

  // Returns true if the process will be launched using base::LaunchOptions.
  // If false, all of the base::LaunchOptions* below will be nullptr.
  // Platform specific.
  bool IsUsingLaunchOptions();

  // Platform specific, returns success or failure. If failure is returned,
  // LaunchOnLauncherThread will not call LaunchProcessOnLauncherThread and
  // AfterLaunchOnLauncherThread, and the launch_result will be reported as
  // LAUNCH_RESULT_FAILURE.
  bool BeforeLaunchOnLauncherThread(FileMappedForLaunch& files_to_register,
                                    base::LaunchOptions* options);

  // Does the actual starting of the process.
  // If IsUsingLaunchOptions() returned false, |options| will be null. In this
  // case base::LaunchProcess() will not be used, but another platform
  // specific mechanism for process launching, like Linux's zygote or
  // Android's app zygote. |is_synchronous_launch| is set to false if the
  // starting of the process is asynchronous (this is the case on Android), in
  // which case the returned Process is not valid (and
  // PostLaunchOnLauncherThread() will provide the process once it is
  // available). Platform specific.
  ChildProcessLauncherHelper::Process LaunchProcessOnLauncherThread(
      const base::LaunchOptions* options,
      std::unique_ptr<FileMappedForLaunch> files_to_register,
#if BUILDFLAG(IS_ANDROID)
      bool is_pre_warmup_required,
#endif
      bool* is_synchronous_launch,
      int* launch_result);

#if BUILDFLAG(IS_WIN)
  // This is the callback target that handles the result from
  // StartSandboxedProcess().
  void FinishStartSandboxedProcessOnLauncherThread(base::Process process,
                                                   DWORD last_error,
                                                   int launch_result);
#endif

  // Called right after the process has been launched, whether it was created
  // successfully or not. If the process launch is asynchronous, the process may
  // not yet be created. Platform specific.
  void AfterLaunchOnLauncherThread(
      const ChildProcessLauncherHelper::Process& process,
      const base::LaunchOptions* options);

  // Called once the process has been created, successfully or not.
  void PostLaunchOnLauncherThread(ChildProcessLauncherHelper::Process process,
#if BUILDFLAG(IS_WIN)
                                  DWORD last_error,
#endif
                                  int launch_result);

  // Posted by PostLaunchOnLauncherThread onto the client thread.
  void PostLaunchOnClientThread(ChildProcessLauncherHelper::Process process,
#if BUILDFLAG(IS_WIN)
                                DWORD last_error,
#endif
                                int error_code);

  // See ChildProcessLauncher::GetChildTerminationInfo for more info.
  ChildProcessTerminationInfo GetTerminationInfo(
      const ChildProcessLauncherHelper::Process& process,
      bool known_dead);

  // Terminates |process|.
  // Returns true if the process was stopped, false if the process had not been
  // started yet or could not be stopped.
  // Note that |exit_code| is not used on Android.
  static bool TerminateProcess(const base::Process& process, int exit_code);

  // Terminates the process with the normal exit code and ensures it has been
  // stopped. By returning a normal exit code this ensures UMA won't treat this
  // as a crash.
  // Returns immediately and perform the work on the launcher thread.
  static void ForceNormalProcessTerminationAsync(
      ChildProcessLauncherHelper::Process process);

#if BUILDFLAG(IS_IOS)
  void OnChildProcessStarted(pid_t process_id,
                             std::unique_ptr<LaunchResult> launch_result);
  void ClearProcessStorage();

#if defined(__OBJC__)
  NSObject* GetProcess();
#endif
#endif

#if BUILDFLAG(IS_ANDROID)
  void OnChildProcessStarted(JNIEnv* env, jint handle);

  base::android::ChildBindingState GetEffectiveChildBindingState();

  // Dumps the stack of the child process without crashing it.
  void DumpProcessStack(const base::Process& process);

  void SetRenderProcessPriorityOnLauncherThread(
      base::Process process,
      const RenderProcessPriority& priority);
#else   // !BUILDFLAG(IS_ANDROID)
  void SetProcessPriorityOnLauncherThread(base::Process process,
                                          base::Process::Priority priority);
#endif  // !BUILDFLAG(IS_ANDROID)

  std::string GetProcessType();

 private:
  friend class base::RefCountedThreadSafe<ChildProcessLauncherHelper>;

  ~ChildProcessLauncherHelper();

  void LaunchOnLauncherThread();

  // Update command line and mapped handles if a log handle is being passed.
  void PassLoggingSwitches(base::LaunchOptions* launch_options,
                           base::CommandLine* cmd_line);

#if BUILDFLAG(USE_ZYGOTE)
  // Returns the zygote handle for this particular launch, if any.
  ZygoteCommunication* GetZygoteForLaunch();
#endif  // BUILDFLAG(USE_ZYGOTE)

  base::CommandLine* command_line() {
    DCHECK(CurrentlyOnProcessLauncherTaskRunner());
    return command_line_.get();
  }
  int child_process_id() const { return child_process_id_; }

  static void ForceNormalProcessTerminationSync(
      ChildProcessLauncherHelper::Process process);

#if BUILDFLAG(IS_ANDROID)
  void set_java_peer_available_on_client_thread() {
    java_peer_avaiable_on_client_thread_ = true;
  }
#endif

  const int child_process_id_;
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  base::TimeTicks begin_launch_time_;
  // Accessed on launcher thread.
  std::unique_ptr<base::CommandLine> command_line_;
  std::unique_ptr<SandboxedProcessLauncherDelegate> delegate_;
  base::WeakPtr<ChildProcessLauncher> child_process_launcher_;

#if BUILDFLAG(IS_CHROMEOS)
  std::optional<base::ProcessId> process_id_ = std::nullopt;
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // The priority of the process. The state is stored to avoid changing the
  // setting repeatedly.
  std::optional<base::Process::Priority> priority_;
#endif

  // The PlatformChannel that will be used to transmit an invitation to the
  // child process in most cases. Only used if the platform's helper
  // implementation doesn't return a server endpoint from
  // |CreateNamedPlatformChannelOnLauncherThread()|.
  std::optional<mojo::PlatformChannel> mojo_channel_;

#if !BUILDFLAG(IS_FUCHSIA)
  // May be used in exclusion to the above if the platform helper implementation
  // returns a valid server endpoint from
  // |CreateNamedPlatformChannelOnLauncherThread()|.
  std::optional<mojo::NamedPlatformChannel> mojo_named_channel_;
#endif

  bool terminate_on_shutdown_;
  mojo::OutgoingInvitation mojo_invitation_;
  const mojo::ProcessErrorCallback process_error_callback_;
  std::unique_ptr<ChildProcessLauncherFileData> file_data_;

#if BUILDFLAG(IS_MAC)
  std::unique_ptr<sandbox::SeatbeltExecClient> seatbelt_exec_client_;
  sandbox::mac::SandboxPolicy policy_;
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_IOS)
  std::unique_ptr<base::MachPortRendezvousServerIOS> rendezvous_server_;
  std::unique_ptr<ProcessStorageBase> process_storage_;
#endif

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_peer_;
  bool java_peer_avaiable_on_client_thread_ = false;
  // Whether the process can use warmed up connection.
  bool can_use_warm_up_connection_;
#endif

#if BUILDFLAG(IS_FUCHSIA)
  std::unique_ptr<sandbox::policy::SandboxPolicyFuchsia> sandbox_policy_;
#endif

#if BUILDFLAG(IS_WIN)
  // Only valid if the host process has logging enabled.
  base::win::ScopedHandle log_handle_;
#endif

  // Histogram shared memory region metadata.
  base::UnsafeSharedMemoryRegion histogram_memory_region_;

  // Startup tracing config shared memory region.
  base::ReadOnlySharedMemoryRegion tracing_config_memory_region_;

  // Creation time of the helper, used for metrics.
  // TODO(crbug.com/40287847): Remove when parallel launching is finished.
  base::TimeTicks init_start_time_;
};

}  // namespace internal

}  // namespace content

#endif  // CONTENT_BROWSER_CHILD_PROCESS_LAUNCHER_HELPER_H_
