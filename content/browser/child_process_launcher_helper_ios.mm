// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher_helper.h"

#import <BrowserEngineKit/BrowserEngineKit.h>

#include <list>

#include "base/apple/mach_port_rendezvous.h"
#include "base/no_destructor.h"
#include "base/threading/platform_thread.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/child_process_launcher_helper_posix.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {
namespace internal {

static base::NoDestructor<base::Lock> g_process_table_lock_;
static base::NoDestructor<
    std::map<pid_t, scoped_refptr<ChildProcessLauncherHelper>>>
    g_process_table_;

void InvalidateProcess(NSObject* process) {
  if ([process isKindOfClass:[BEWebContentProcess class]]) {
    [(BEWebContentProcess*)process invalidate];
  } else if ([process isKindOfClass:[BENetworkingProcess class]]) {
    [(BENetworkingProcess*)process invalidate];
  } else if ([process isKindOfClass:[BERenderingProcess class]]) {
    [(BERenderingProcess*)process invalidate];
  }
}

void OnChildProcessTerminatedOnAnyThread(pid_t process_id) {
  base::AutoLock guard(*g_process_table_lock_);
  auto it = g_process_table_->find(process_id);
  if (it != g_process_table_->end()) {
    it->second->ClearProcessStorage();
    g_process_table_->erase(it);
  }
}

bool TerminateNow(pid_t process_id) {
  NSObject* process = nullptr;
  {
    base::AutoLock guard(*g_process_table_lock_);
    auto it = g_process_table_->find(process_id);
    if (it != g_process_table_->end()) {
      process = it->second->GetProcess();
    }
  }

  if (!process) {
    return false;
  }
  InvalidateProcess(process);
  return true;
}

bool WaitForExit(pid_t process_id, int* exit_code, base::TimeDelta timeout) {
  base::TimeTicks wakeup_time = base::TimeTicks::Now() + timeout;
  constexpr uint32_t kMaxSleepInMicroseconds = 1 << 18;  // ~256 ms.
  uint32_t max_sleep_time_usecs = 1 << 10;               // ~1 ms.
  int double_sleep_time = 0;

  while (true) {
    {
      base::AutoLock guard(*g_process_table_lock_);
      auto it = g_process_table_->find(process_id);
      if (it != g_process_table_->end()) {
        if (it->second->GetProcess() == nullptr) {
          if (exit_code) {
            *exit_code = 0;
          }
          return true;
        }
      } else {
        return true;
      }
    }
    base::TimeTicks now = base::TimeTicks::Now();
    if (now > wakeup_time) {
      return false;
    }

    const uint32_t sleep_time_usecs = static_cast<uint32_t>(
        std::min(static_cast<uint64_t>((wakeup_time - now).InMicroseconds()),
                 uint64_t{max_sleep_time_usecs}));
    base::PlatformThread::Sleep(base::Microseconds(sleep_time_usecs));
    if ((max_sleep_time_usecs < kMaxSleepInMicroseconds) &&
        (double_sleep_time++ % 4 == 0)) {
      max_sleep_time_usecs *= 2;
    }
  }
}

// Object used to pass the result of the launch from the async
// dispatch_queue to the LauncherThread.
class LaunchResult {
 public:
  void Invalidate() { InvalidateProcess(process); }

  id<BEProcessCapabilityGrant> GrantForeground(NSError** error) {
    id<BEProcessCapabilityGrant> grant;
    BEProcessCapability* cap = [BEProcessCapability foreground];
    if ([process isKindOfClass:[BEWebContentProcess class]]) {
      grant = [(BEWebContentProcess*)process grantCapability:cap error:error];
    } else if ([process isKindOfClass:[BENetworkingProcess class]]) {
      grant = [(BENetworkingProcess*)process grantCapability:cap error:error];
    } else if ([process isKindOfClass:[BERenderingProcess class]]) {
      grant = [(BERenderingProcess*)process grantCapability:cap error:error];
    }
    return grant;
  }

  xpc_connection_t CreateXPCConnection(NSError** error) {
    if ([process isKindOfClass:[BEWebContentProcess class]]) {
      return [(BEWebContentProcess*)process makeLibXPCConnectionError:error];
    } else if ([process isKindOfClass:[BENetworkingProcess class]]) {
      return [(BENetworkingProcess*)process makeLibXPCConnectionError:error];
    } else if ([process isKindOfClass:[BERenderingProcess class]]) {
      return [(BERenderingProcess*)process makeLibXPCConnectionError:error];
    }
    return {};
  }

  NSObject* process;
  NSError* launch_error;
};

// Object to store the process handles.
class ProcessStorage : public ProcessStorageBase {
 public:
  ProcessStorage(NSObject* process,
                 xpc_connection_t connection,
                 id<BEProcessCapabilityGrant> grant)
      : process_(process), ipc_channel_(connection), grant_(grant) {}

  ~ProcessStorage() override { [grant_ invalidate]; }

  void ReleaseProcess() override { process_ = nullptr; }

  NSObject* Process() { return process_; }

 private:
  NSObject* process_;
  [[maybe_unused]] xpc_connection_t ipc_channel_;
  id<BEProcessCapabilityGrant> grant_;
};

std::optional<mojo::NamedPlatformChannel>
ChildProcessLauncherHelper::CreateNamedPlatformChannelOnLauncherThread() {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  return std::nullopt;
}

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
}

std::unique_ptr<PosixFileDescriptorInfo>
ChildProcessLauncherHelper::GetFilesToMap() {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  return CreateDefaultPosixFilesToMap(
      child_process_id(), mojo_channel_->remote_endpoint(),
      /*files_to_preload=*/{}, GetProcessType(), command_line());
}

bool ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    FileMappedForLaunch& files_to_register,
    base::LaunchOptions* options) {
  mojo::PlatformHandle endpoint =
      mojo_channel_->TakeRemoteEndpoint().TakePlatformHandle();
  DCHECK(endpoint.is_valid_mach_receive());
  options->mach_ports_for_rendezvous.insert(std::make_pair(
      'mojo', base::MachRendezvousPort(endpoint.TakeMachReceiveRight())));
  return true;
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions* options,
    std::unique_ptr<PosixFileDescriptorInfo> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  DCHECK(options);
  *is_synchronous_launch = false;
  rendezvous_server_ = std::make_unique<base::MachPortRendezvousServerIOS>(
      options->mach_ports_for_rendezvous);

  // We need to hand out unique "process ids" just use a static counter
  // for now. There should only be one launcher thread so this is
  // synchronous access it doesn't need to be an atomic.
  static pid_t g_pid = 0;
  pid_t process_id = ++g_pid;

  static bool g_hooks_registered = false;
  if (!g_hooks_registered) {
    base::Process::SetTerminationHooks(&TerminateNow, &WaitForExit);
    g_hooks_registered = true;
  }

  void (^process_terminated)() = ^void() {
    OnChildProcessTerminatedOnAnyThread(process_id);
  };
  std::string process_type = GetProcessType();
  std::string utility_sub_type =
      command_line()->GetSwitchValueASCII(switches::kUtilitySubType);
  if (process_type == switches::kUtilityProcess &&
      utility_sub_type == network::mojom::NetworkService::Name_) {
    void (^process_launch_complete)(BENetworkingProcess* process,
                                    NSError* error) =
        ^void(BENetworkingProcess* process, NSError* error) {
          auto result = std::make_unique<LaunchResult>(process, error);
          GetProcessLauncherTaskRunner()->PostTask(
              FROM_HERE,
              base::BindOnce(&ChildProcessLauncherHelper::OnChildProcessStarted,
                             this, process_id, std::move(result)));
        };

    [BENetworkingProcess
        networkProcessWithInterruptionHandler:process_terminated
                                   completion:process_launch_complete];

  } else if (process_type == switches::kGpuProcess) {
    void (^process_launch_complete)(BERenderingProcess* process,
                                    NSError* error) =
        ^void(BERenderingProcess* process, NSError* error) {
          auto result = std::make_unique<LaunchResult>(process, error);
          GetProcessLauncherTaskRunner()->PostTask(
              FROM_HERE,
              base::BindOnce(&ChildProcessLauncherHelper::OnChildProcessStarted,
                             this, process_id, std::move(result)));
        };

    [BERenderingProcess
        renderingProcessWithInterruptionHandler:process_terminated
                                     completion:process_launch_complete];
  } else {
    // This can be both kUtility and kRenderProcess.
    void (^process_launch_complete)(BEWebContentProcess* process,
                                    NSError* error) =
        ^void(BEWebContentProcess* process, NSError* error) {
          auto result = std::make_unique<LaunchResult>(process, error);
          GetProcessLauncherTaskRunner()->PostTask(
              FROM_HERE,
              base::BindOnce(&ChildProcessLauncherHelper::OnChildProcessStarted,
                             this, process_id, std::move(result)));
        };

    [BEWebContentProcess
        webContentProcessWithInterruptionHandler:process_terminated
                                      completion:process_launch_complete];
  }
  AddRef();
  return Process();
}

void ChildProcessLauncherHelper::OnChildProcessStarted(
    pid_t process_id,
    std::unique_ptr<LaunchResult> launch_result) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  scoped_refptr<ChildProcessLauncherHelper> ref(this);
  Release();  // Balances with LaunchProcessOnLauncherThread.

  int launch_result_code = LAUNCH_RESULT_FAILURE;

  if (!launch_result->launch_error) {
    NSError* error = nil;

    // TODO(dtapuska): For now we grant everything foreground capability. We
    // need to hook this grant up to the
    // `RenderProcessHostImpl::UpdateProcessPriority()`.
    id<BEProcessCapabilityGrant> grant = launch_result->GrantForeground(&error);

    xpc_connection_t xpc_connection =
        launch_result->CreateXPCConnection(&error);
    if (xpc_connection) {
      xpc_connection_set_event_handler(xpc_connection, ^(xpc_object_t event) {
        if (event == XPC_ERROR_CONNECTION_INTERRUPTED) {
          OnChildProcessTerminatedOnAnyThread(process_id);
        }
      });
      xpc_connection_resume(xpc_connection);
      xpc_object_t message = xpc_dictionary_create(nil, nil, 0);
      xpc_object_t args_array = xpc_array_create_empty();
      for (const auto& arg : command_line()->argv()) {
        xpc_object_t value = xpc_string_create(arg.c_str());
        xpc_array_append_value(args_array, value);
      }
      xpc_dictionary_set_value(message, "args", args_array);
      xpc_dictionary_set_mach_send(
          message, "port", rendezvous_server_->GetMachSendRight().get());
      xpc_connection_send_message(xpc_connection, message);
      launch_result_code = LAUNCH_RESULT_SUCCESS;

      // Keep reference to process, xpc_connection and the grant for the process
      // life.
      process_storage_ = std::make_unique<ProcessStorage>(
          launch_result->process, xpc_connection, grant);

      // Add the process to the global table.
      {
        base::AutoLock guard(*g_process_table_lock_);
        CHECK(!base::Contains(*g_process_table_, process_id));
        g_process_table_->emplace(process_id, this);
      }

    } else {
      [grant invalidate];
      launch_result->Invalidate();
    }
  }

  ChildProcessLauncherHelper::Process process;
  process.process = base::Process(process_id);
#if TARGET_OS_SIMULATOR
  process.process.SetIsContentProcess();
#endif
  PostLaunchOnLauncherThread(std::move(process), launch_result_code);
}

bool ChildProcessLauncherHelper::IsUsingLaunchOptions() {
  return true;
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions* options) {}

ChildProcessTerminationInfo ChildProcessLauncherHelper::GetTerminationInfo(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead) {
  ChildProcessTerminationInfo info;
  if (!process_storage_) {
    info.status = base::TERMINATION_STATUS_LAUNCH_FAILED;
  } else if (static_cast<ProcessStorage*>(process_storage_.get())->Process() ==
             nullptr) {
    info.status = base::TERMINATION_STATUS_NORMAL_TERMINATION;
  } else {
    info.status = base::TERMINATION_STATUS_STILL_RUNNING;
  }
  return info;
}

void ChildProcessLauncherHelper::ClearProcessStorage() {
  if (process_storage_) {
    process_storage_->ReleaseProcess();
  }
}

NSObject* ChildProcessLauncherHelper::GetProcess() {
  if (process_storage_) {
    return static_cast<ProcessStorage*>(process_storage_.get())->Process();
  }
  return nullptr;
}

// static
bool ChildProcessLauncherHelper::TerminateProcess(const base::Process& process,
                                                  int exit_code) {
  // TODO(crbug.com/40565504): Determine whether we should also call
  // EnsureProcessTerminated() to make sure of process-exit, and reap it.
  return process.Terminate(exit_code, false);
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  // Client has gone away, so just kill the process.  Using exit code 0 means
  // that UMA won't treat this as a crash.
  process.process.Terminate(RESULT_CODE_NORMAL_EXIT, false);
  base::EnsureProcessTerminated(std::move(process.process));
}

void ChildProcessLauncherHelper::SetProcessPriorityOnLauncherThread(
    base::Process process,
    base::Process::Priority priority) {}

// static
base::File OpenFileToShare(const base::FilePath& path,
                           base::MemoryMappedFile::Region* region) {
  // Not used yet (until required files are described in the service manifest on
  // iOS).
  NOTREACHED_IN_MIGRATION();
  return base::File();
}

}  //  namespace internal
}  // namespace content
