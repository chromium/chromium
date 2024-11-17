// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/service_process_launcher.h"

#include <utility>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "services/service_manager/public/cpp/service_executable/switches.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/switches.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/mach_port_rendezvous.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "sandbox/linux/services/namespace_sandbox.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/windows_version.h"
#endif

namespace service_manager {

// Thread-safe owner of state related to a service process. This facilitates
// safely scheduling the launching and joining of a service process in the
// background.
class ServiceProcessLauncher::ProcessState
    : public base::RefCountedThreadSafe<ProcessState> {
 public:
  ProcessState() { DETACH_FROM_SEQUENCE(sequence_checker_); }

  ProcessState(const ProcessState&) = delete;
  ProcessState& operator=(const ProcessState&) = delete;

  base::ProcessId LaunchInBackground(
      const Identity& target,
      sandbox::mojom::Sandbox sandbox_type,
      std::unique_ptr<base::CommandLine> child_command_line,
      mojo::PlatformChannel::HandlePassingInfo handle_passing_info,
      mojo::PlatformChannel channel,
      mojo::OutgoingInvitation invitation);

  void StopInBackground();

 private:
  friend class base::RefCountedThreadSafe<ProcessState>;

  ~ProcessState() = default;

  base::Process child_process_;
  SEQUENCE_CHECKER(sequence_checker_);
};

ServiceProcessLauncher::ServiceProcessLauncher(
    ServiceProcessLauncherDelegate* delegate,
    const base::FilePath& service_path)
    : delegate_(delegate),
      service_path_(service_path.empty()
                        ? base::CommandLine::ForCurrentProcess()->GetProgram()
                        : service_path),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE, base::WithBaseSyncPrimitives(),
           base::MayBlock()})) {}

ServiceProcessLauncher::~ServiceProcessLauncher() {
  // We don't really need to wait for the process to be joined, as long as it
  // eventually happens. Schedule a task to do it, and move on.
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ProcessState::StopInBackground, state_));
}

mojo::PendingRemote<mojom::Service> ServiceProcessLauncher::Start(
    const Identity& target,
    sandbox::mojom::Sandbox sandbox_type,
    ProcessReadyCallback callback) {
  DCHECK(!state_);

  const base::CommandLine& parent_command_line =
      *base::CommandLine::ForCurrentProcess();

  std::unique_ptr<base::CommandLine> child_command_line(
      new base::CommandLine(service_path_));

  child_command_line->AppendArguments(parent_command_line, false);

  // Add enabled/disabled features from base::FeatureList. These will take
  // precedence over existing ones (if there is any copied from the
  // |parent_command_line| above) as they appear later in the arguments list.
  std::string enabled_features;
  std::string disabled_features;
  base::FeatureList::GetInstance()->GetFeatureOverrides(&enabled_features,
                                                        &disabled_features);
  if (!enabled_features.empty()) {
    child_command_line->AppendSwitchASCII(::switches::kEnableFeatures,
                                          enabled_features);
  }
  if (!disabled_features.empty()) {
    child_command_line->AppendSwitchASCII(::switches::kDisableFeatures,
                                          disabled_features);
  }

  // Use --force-field-trials to make the child process to create field trials.
  std::string field_trial_states;
  base::FieldTrialList::AllStatesToString(&field_trial_states);
  if (!field_trial_states.empty()) {
    DCHECK(!child_command_line->HasSwitch(::switches::kForceFieldTrials));
    child_command_line->AppendSwitchASCII(::switches::kForceFieldTrials,
                                          field_trial_states);
  }

  child_command_line->AppendSwitchASCII(switches::kServiceName, target.name());
#ifndef NDEBUG
  child_command_line->AppendSwitchASCII("g",
                                        target.instance_group().ToString());
#endif

  if (!sandbox::policy::IsUnsandboxedSandboxType(sandbox_type)) {
    child_command_line->AppendSwitchASCII(
        sandbox::policy::switches::kServiceSandboxType,
        sandbox::policy::StringFromUtilitySandboxType(sandbox_type));
  }

  mojo::PlatformChannel::HandlePassingInfo handle_passing_info;
  mojo::PlatformChannel channel;
  channel.PrepareToPassRemoteEndpoint(&handle_passing_info,
                                      child_command_line.get());
  mojo::OutgoingInvitation invitation;
  auto client =
      PassServiceRequestOnCommandLine(&invitation, child_command_line.get());

  if (delegate_) {
    delegate_->AdjustCommandLineArgumentsForTarget(target,
                                                   child_command_line.get());
  }

  state_ = base::WrapRefCounted(new ProcessState);
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ProcessState::LaunchInBackground, state_, target,
                     sandbox_type, std::move(child_command_line),
                     std::move(handle_passing_info), std::move(channel),
                     std::move(invitation)),
      std::move(callback));

  return client;
}

// static
mojo::PendingRemote<mojom::Service>
ServiceProcessLauncher::PassServiceRequestOnCommandLine(
    mojo::OutgoingInvitation* invitation,
    base::CommandLine* command_line) {
  const auto attachment_name = base::NumberToString(base::RandUint64());
  command_line->AppendSwitchASCII(switches::kServiceRequestAttachmentName,
                                  attachment_name);
  return mojo::PendingRemote<mojom::Service>(
      invitation->AttachMessagePipe(attachment_name), 0);
}

base::ProcessId ServiceProcessLauncher::ProcessState::LaunchInBackground(
    const Identity& target,
    sandbox::mojom::Sandbox sandbox_type,
    std::unique_ptr<base::CommandLine> child_command_line,
    mojo::PlatformChannel::HandlePassingInfo handle_passing_info,
    mojo::PlatformChannel channel,
    mojo::OutgoingInvitation invitation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::LaunchOptions options;
#if BUILDFLAG(IS_WIN)
  options.handles_to_inherit = handle_passing_info;
  options.stdin_handle = INVALID_HANDLE_VALUE;
  options.stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  options.stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
  // Always inherit stdout/stderr as a pair.
  if (!options.stdout_handle || !options.stdin_handle)
    options.stdin_handle = options.stdout_handle = nullptr;

  // Pseudo handles are used when stdout and stderr redirect to the console. In
  // that case, they're automatically inherited by child processes. See
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms682075.aspx
  // Trying to add them to the list of handles to inherit causes CreateProcess
  // to fail. When this process is launched from Python then a real handle is
  // used. In that case, we do want to add it to the list of handles that is
  // inherited.
  if (options.stdout_handle &&
      GetFileType(options.stdout_handle) != FILE_TYPE_CHAR) {
    options.handles_to_inherit.push_back(options.stdout_handle);
  }
  if (options.stderr_handle &&
      GetFileType(options.stderr_handle) != FILE_TYPE_CHAR &&
      options.stdout_handle != options.stderr_handle) {
    options.handles_to_inherit.push_back(options.stderr_handle);
  }
#elif BUILDFLAG(IS_FUCHSIA)
  // LaunchProcess will share stdin/out/err with the child process by default.
  if (!sandbox::policy::IsUnsandboxedSandboxType(sandbox_type))
    NOTIMPLEMENTED();
  options.handles_to_transfer = std::move(handle_passing_info);
#elif BUILDFLAG(IS_POSIX)
  const base::FileHandleMappingVector fd_mapping{
      {STDIN_FILENO, STDIN_FILENO},
      {STDOUT_FILENO, STDOUT_FILENO},
      {STDERR_FILENO, STDERR_FILENO},
  };
#if BUILDFLAG(IS_MAC)
  options.fds_to_remap = fd_mapping;
  options.mach_ports_for_rendezvous = handle_passing_info;
#else
  handle_passing_info.insert(handle_passing_info.end(), fd_mapping.begin(),
                             fd_mapping.end());
  options.fds_to_remap = handle_passing_info;
#endif  // BUILDFLAG(IS_MAC)
#endif
  DVLOG(2) << "Launching child with command line: "
           << child_command_line->GetCommandLineString();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (!sandbox::policy::IsUnsandboxedSandboxType(sandbox_type)) {
    child_process_ =
        sandbox::NamespaceSandbox::LaunchProcess(*child_command_line, options);
    if (!child_process_.IsValid())
      LOG(ERROR) << "Starting the process with a sandbox failed.";
  } else
#endif
  {
    child_process_ = base::LaunchProcess(*child_command_line, options);
  }

  channel.RemoteProcessLaunchAttempted();
  if (!child_process_.IsValid()) {
    LOG(ERROR) << "Failed to start child process for service: "
               << target.name();
    return base::kNullProcessId;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Always log instead of DVLOG because knowing which pid maps to which
  // service is vital for interpreting crashes after-the-fact and Chrome OS
  // devices generally run release builds, even in development.
  VLOG(0)
#else
  DVLOG(0)
#endif
      << "Launched child process pid=" << child_process_.Pid()
      << " id=" << target.ToString();

  mojo::OutgoingInvitation::Send(std::move(invitation), child_process_.Handle(),
                                 channel.TakeLocalEndpoint());

  return child_process_.Pid();
}

void ServiceProcessLauncher::ProcessState::StopInBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!child_process_.IsValid())
    return;

  int rv = -1;
  LOG_IF(ERROR, !child_process_.WaitForExit(&rv))
      << "Failed to wait for child process";
  child_process_.Close();
}

}  // namespace service_manager
