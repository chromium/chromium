// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher_helper.h"

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/apple/mach_port_rendezvous.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/posix/global_descriptors.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/child_process_launcher_helper_posix.h"
#include "content/browser/child_process_task_port_provider_mac.h"
#include "content/browser/sandbox_parameters_mac.h"
#include "content/browser/sandboxed_process_launcher_delegate.h"
#include "content/grit/content_resources.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "sandbox/mac/sandbox_serializer.h"
#include "sandbox/mac/seatbelt_exec.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/mac/sandbox_mac.h"
#include "sandbox/policy/sandbox.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"

namespace content {
namespace internal {

namespace {

// Class that holds a map of SandboxTypes to serialized policy strings. Only
// certain sandbox types can be cached, depending on the nature of the
// runtime parameters that are bound into the profile.
class SandboxProfileCache {
 public:
  SandboxProfileCache() = default;
  ~SandboxProfileCache() = default;

  static SandboxProfileCache& Get() {
    static base::NoDestructor<SandboxProfileCache> cache;
    return *cache;
  }

  const std::string* Query(sandbox::mojom::Sandbox sandbox_type) {
    base::AutoLock lock(lock_);
    auto it = cache_.find(sandbox_type);
    if (it == cache_.end())
      return nullptr;
    return &it->second;
  }

  void Insert(sandbox::mojom::Sandbox sandbox_type, const std::string& policy) {
    DCHECK(sandbox::policy::CanCacheSandboxPolicy(sandbox_type));
    base::AutoLock lock(lock_);
    cache_.emplace(sandbox_type, policy);
  }

 private:
  base::Lock lock_;
  base::flat_map<sandbox::mojom::Sandbox, std::string> cache_ GUARDED_BY(lock_);
};

std::unique_ptr<base::ScopedTempDir> CreateSandboxChildDir(
    base::DarwinUserDirectory directory,
    const std::string& suffix) {
  base::FilePath parent_dir = base::GetDarwinUserDirectory(directory);
  CHECK(!parent_dir.empty());

  base::FilePath path = parent_dir.Append(suffix);
  auto scoped_dir = std::make_unique<base::ScopedTempDir>();
  CHECK(scoped_dir->CreateDirectoryExclusive(path))
      << "Failed to create process-isolated directory at " << path;
  return scoped_dir;
}

}  // namespace

std::string GetProcessIsolatedDarwinUserDirSuffix() {
  return base::StrCat({base::apple::BaseBundleID(), ".",
                       base::NumberToString(base::GetCurrentProcId()),
                       ".child"});
}

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

bool ChildProcessLauncherHelper::IsUsingLaunchOptions() {
  return true;
}

bool ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    FileMappedForLaunch& files_to_register,
    base::LaunchOptions* options) {
  // Convert FD mapping to FileHandleMappingVector.
  options->fds_to_remap = files_to_register.GetMappingWithIDAdjustment(
      base::GlobalDescriptors::kBaseDescriptor);

  mojo::PlatformHandle endpoint =
      mojo_channel_->TakeRemoteEndpoint().TakePlatformHandle();
  DCHECK(endpoint.is_valid_mach_receive());
  options->mach_ports_for_rendezvous.insert(std::make_pair(
      'mojo', base::MachRendezvousPort(endpoint.TakeMachReceiveRight())));

  options->environment = delegate_->GetEnvironment();

  options->disclaim_responsibility = delegate_->DisclaimResponsibility();
  options->enable_cpu_security_mitigations =
      delegate_->EnableCpuSecurityMitigations();
  options->process_requirement = delegate_->GetProcessRequirement();

  auto sandbox_type =
      sandbox::policy::SandboxTypeFromCommandLine(*command_line_);

  bool no_sandbox =
      command_line_->HasSwitch(sandbox::policy::switches::kNoSandbox) ||
      sandbox::policy::IsUnsandboxedSandboxType(sandbox_type);

  if (!no_sandbox) {
    if (delegate_->NeedsProcessIsolatedDarwinUserDirs() &&
        !CreateProcessIsolatedDarwinUserDirs(options)) {
      return false;
    }

    if (!LOG_IS_ON(INFO)) {
      // Disable os logging to com.apple.diagnosticd when logging is not
      // enabled. The system logging has a measureable performance impact.
      options->environment.insert(
          std::make_pair("OS_ACTIVITY_MODE", "disable"));
    }

    const std::string* cached_policy =
        SandboxProfileCache::Get().Query(sandbox_type);
    if (cached_policy) {
      serialized_policy_ = *cached_policy;
    } else {
      const bool can_cache_policy =
          sandbox::policy::CanCacheSandboxPolicy(sandbox_type);

      // Generate the sandbox policy profile.
      sandbox::SandboxSerializer compiler(
          can_cache_policy ? sandbox::SandboxSerializer::Target::kCompiled
                           : sandbox::SandboxSerializer::Target::kSource);
      compiler.SetProfile(sandbox::policy::GetSandboxProfile(sandbox_type));
      const bool sandbox_ok = SetupSandboxParameters(
          sandbox_type, *command_line_.get(), options->environment, &compiler);

      if (!sandbox_ok) {
        LOG(ERROR) << "Sandbox setup failed.";
        return false;
      }

      std::string error;
      if (!compiler.SerializePolicy(serialized_policy_, error)) {
        LOG(ERROR) << "Failed to compile sandbox policy: " << error;
        return false;
      }

      if (can_cache_policy) {
        SandboxProfileCache::Get().Insert(sandbox_type, serialized_policy_);
      }
    }

    seatbelt_exec_client_ = std::make_unique<sandbox::SeatbeltExecClient>();
    int pipe = seatbelt_exec_client_->GetReadFD();
    if (pipe < 0) {
      LOG(ERROR) << "The file descriptor for the sandboxed child is invalid.";
      return false;
    }

    options->fds_to_remap.push_back(std::make_pair(pipe, pipe));

    // Update the command line to enable the V2 sandbox and pass the
    // communication FD to the helper executable.
    command_line_->AppendArg(
        base::StringPrintf("%s%d", sandbox::switches::kSeatbeltClient, pipe));
  }

  return true;
}

bool ChildProcessLauncherHelper::CreateProcessIsolatedDarwinUserDirs(
    base::LaunchOptions* options) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());

  base::FilePath darwin_user_temp_dir =
      base::GetDarwinUserDirectory(base::DarwinUserDirectory::kUserTemp);
  std::string user_dir_suffix = GetProcessIsolatedDarwinUserDirSuffix();

  // Retry in the event that the directory name created by
  // CreateUniqueTempDirUnderPath exists in one of the other darwin user dirs.
  const int kMaxTries = 3;
  for (int i = 0; i < kMaxTries; i++) {
    auto temp_user_temp_dir = std::make_unique<base::ScopedTempDir>();
    if (!temp_user_temp_dir->CreateUniqueTempDirUnderPath(darwin_user_temp_dir,
                                                          user_dir_suffix)) {
      continue;
    }

    std::string last_path_component =
        temp_user_temp_dir->GetPath().BaseName().value();

    std::unique_ptr<base::ScopedTempDir> temp_user_dir = CreateSandboxChildDir(
        base::DarwinUserDirectory::kUser, last_path_component);
    if (!temp_user_dir) {
      continue;
    }

    std::unique_ptr<base::ScopedTempDir> temp_user_cache_dir =
        CreateSandboxChildDir(base::DarwinUserDirectory::kUserCache,
                              last_path_component);
    if (!temp_user_cache_dir) {
      continue;
    }

    std::unique_ptr<base::Environment> env(base::Environment::Create());
    CHECK(!env->GetVar(base::env_vars::kDirHelperUserDirSuffix).has_value())
        << base::env_vars::kDirHelperUserDirSuffix
        << " is used to create process isolated subdirectories for child "
           "processes and cannot be set";

    // Set DIRHELPER_USER_DIR_SUFFIX to customize the path provided by
    // NSTemporaryDirectory() and other paths retrieved from confstr.
    //
    // Note that DIRHELPER_USER_DIR_SUFFIX *does not* support nested paths.
    // Providing more than one path component will cause this environment
    // variable to be ignored.
    options->environment[base::env_vars::kDirHelperUserDirSuffix] =
        last_path_component;

    // Prevent the child process from inheriting MAC_CHROMIUM_TMPDIR if set,
    // ensuring base::GetTempDir() in the child process falls back to
    // NSTemporaryDirectory(), which respects DIRHELPER_USER_DIR_SUFFIX.
    options->environment[base::env_vars::kMacChromiumTmpDir] = "";

    // Set TMPDIR to the isolated temp directory path so code that inspects
    // TMPDIR directly uses the permitted directory.
    options->environment[base::env_vars::kTmpDir] =
        temp_user_temp_dir->GetPath().value();

    scoped_sandboxed_user_dir_ = std::move(temp_user_dir);
    scoped_sandboxed_user_cache_dir_ = std::move(temp_user_cache_dir);
    scoped_sandboxed_user_temp_dir_ = std::move(temp_user_temp_dir);
    return true;
  }

  return false;
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions* options,
    std::unique_ptr<PosixFileDescriptorInfo> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  *is_synchronous_launch = true;
  ChildProcessLauncherHelper::Process process;
  process.process = base::LaunchProcess(*command_line(), *options);
  *launch_result = process.process.IsValid() ? LAUNCH_RESULT_SUCCESS
                                             : LAUNCH_RESULT_FAILURE;
  return process;
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions* options) {
  // Send the sandbox profile after launch so that the child will exist and be
  // waiting for the message on its side of the pipe.
  if (process.process.IsValid() && seatbelt_exec_client_.get() != nullptr) {
    seatbelt_exec_client_->SendPolicy(serialized_policy_);
  }
}

ChildProcessTerminationInfo ChildProcessLauncherHelper::GetTerminationInfo(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead) {
  ChildProcessTerminationInfo info;
  info.status = known_dead ? base::GetKnownDeadTerminationStatus(
                                 process.process.Handle(), &info.exit_code)
                           : base::GetTerminationStatus(
                                 process.process.Handle(), &info.exit_code);
  return info;
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
    base::Process::Priority priority) {
  if (process.CanSetPriority()) {
    process.SetPriority(ChildProcessTaskPortProvider::GetInstance(), priority);
  }
}

base::File OpenFileToShare(const base::FilePath& path,
                           base::MemoryMappedFile::Region* region) {
  // Not used yet (until required files are described in the service manifest on
  // Mac).
  NOTREACHED();
}

}  //  namespace internal
}  // namespace content
