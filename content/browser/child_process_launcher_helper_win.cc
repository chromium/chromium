// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/child_process_launcher_helper.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandbox_init_win.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/sandbox_types.h"

namespace {

// /prefetch:# arguments to use when launching various process types. It has
// been observed that when file reads are consistent for 3 process launches with
// the same /prefetch:# argument, the Windows prefetcher starts issuing reads in
// batch at process launch. Because reads depend on the process type, the
// prefetcher wouldn't be able to observe consistent reads if no /prefetch:#
// arguments were used. Note that the browser process has no /prefetch:#
// argument; as such all other processes must have one in order to avoid
// polluting its profile.

// On Windows versions before Win11 21H2 the value must always be in [1, 8];
// otherwise it is treated as 0 by the Windows prefetcher and will interfere
// with the main process launch.

constexpr std::string_view kPrefetchArgument1 = "/prefetch:1";
constexpr std::string_view kPrefetchArgument2 = "/prefetch:2";
constexpr std::string_view kPrefetchArgument3 = "/prefetch:3";
constexpr std::string_view kPrefetchArgument4 = "/prefetch:4";

// /prefetch:5, /prefetch:6 and /prefetch:7 are reserved for content embedders
// and are not to be used by content itself. There are two exceptions to this
// rule.
//
// We violate this rule with kBrowserBackground using 5 defined by
// kPrefetchArgumentBrowserBackground in chrome/common/chrome_switches.cc.

constexpr std::string_view kPrefetchArgument5 = "/prefetch:5";
// constexpr std::string_view kPrefetchArgument6 = "/prefetch:6";
// constexpr std::string_view kPrefetchArgument7 = "/prefetch:7";

// Catch all for Windows versions before Win 11 21H2

constexpr std::string_view kPrefetchArgument8 = "/prefetch:8";

// On Windows 11 21H2 and later the prefetch range was expanded to be [1,16]

constexpr std::string_view kPrefetchArgument9 = "/prefetch:9";
constexpr std::string_view kPrefetchArgument10 = "/prefetch:10";
constexpr std::string_view kPrefetchArgument11 = "/prefetch:11";
constexpr std::string_view kPrefetchArgument12 = "/prefetch:12";
constexpr std::string_view kPrefetchArgument13 = "/prefetch:13";
constexpr std::string_view kPrefetchArgument14 = "/prefetch:14";
// constexpr std::string_view kPrefetchArgument15 = "/prefetch:15";

// Catch all for Windows versions  Win 11 21H2 and later

constexpr std::string_view kPrefetchArgument16 = "/prefetch:16";
}  // namespace

namespace content {
namespace internal {

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {
  DCHECK(client_task_runner_->RunsTasksInCurrentSequence());
}

std::optional<mojo::NamedPlatformChannel>
ChildProcessLauncherHelper::CreateNamedPlatformChannelOnLauncherThread() {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());

  if (!delegate_->ShouldLaunchElevated())
    return std::nullopt;

  mojo::NamedPlatformChannel::Options options;
  mojo::NamedPlatformChannel named_channel(options);
  named_channel.PassServerNameOnCommandLine(command_line());
  return named_channel;
}

std::unique_ptr<FileMappedForLaunch>
ChildProcessLauncherHelper::GetFilesToMap() {
  return nullptr;
}

// static
std::string_view ChildProcessLauncherHelper::GetPrefetchSwitch(
    const AppLaunchPrefetchType prefetch_type) {
  if (base::win::GetVersion() >= base::win::Version::WIN11 &&
      base::FeatureList::IsEnabled(features::kExpandedPrefetchRange)) {
    // These are the prefetch arguments used on Windows versions
    // for Win11 and later. There are fewer processes using the same
    // values and this should lead to better App Launch PreFetch (ALPF)
    // behavior.

    // kPrefetchArgument8 and kPrefetchArgument15 are currently unused.

    switch (prefetch_type) {
      case AppLaunchPrefetchType::kBrowser:
        NOTREACHED_NORETURN();
      case AppLaunchPrefetchType::kRenderer:
        return kPrefetchArgument1;
      case AppLaunchPrefetchType::kGPU:
        return kPrefetchArgument2;
      case AppLaunchPrefetchType::kPpapi:
        return kPrefetchArgument3;
      case AppLaunchPrefetchType::kCrashpad:
        return kPrefetchArgument4;
      case AppLaunchPrefetchType::kBrowserBackground:
        return kPrefetchArgument5;
      case AppLaunchPrefetchType::kExtension:
        return kPrefetchArgument9;
      case AppLaunchPrefetchType::kGPUInfo:
        return kPrefetchArgument10;
      case AppLaunchPrefetchType::kUtilityNetworkService:
        return kPrefetchArgument11;
      case AppLaunchPrefetchType::kUtilityAudio:
        return kPrefetchArgument12;
      case AppLaunchPrefetchType::kUtilityStorage:
        return kPrefetchArgument13;
      case AppLaunchPrefetchType::kUtilityOther:
        return kPrefetchArgument14;
      case AppLaunchPrefetchType::kCatchAll:
        return kPrefetchArgument16;
    }
  } else {
    // These are the prefetch arguments used on Windows versions
    // before Win11 21H2. There are multiple processes using the same values
    // and this leads to less than optimal App Launch PreFetch (ALPF) behavior.

    // /prefetch:5, /prefetch:6 and /prefetch:7 are reserved for content
    // embedders and are not to be used by content itself. We violate this
    // rule with kBrowserBackground using 5 defined by
    // kPrefetchArgumentBrowserBackground in chrome/common/chrome_switches.cc.
    switch (prefetch_type) {
      case AppLaunchPrefetchType::kBrowser:
        NOTREACHED_NORETURN();
      case AppLaunchPrefetchType::kRenderer:
        return kPrefetchArgument1;
      case AppLaunchPrefetchType::kGPU:
        return kPrefetchArgument2;
      case AppLaunchPrefetchType::kExtension:
        return kPrefetchArgument2;
      case AppLaunchPrefetchType::kPpapi:
        return kPrefetchArgument3;
      case AppLaunchPrefetchType::kUtilityNetworkService:
        return kPrefetchArgument3;
      case AppLaunchPrefetchType::kCrashpad:
        return kPrefetchArgument4;
      case AppLaunchPrefetchType::kBrowserBackground:
        return kPrefetchArgument5;
      case AppLaunchPrefetchType::kCatchAll:
        return kPrefetchArgument8;
      case AppLaunchPrefetchType::kGPUInfo:
        return kPrefetchArgument8;
      case AppLaunchPrefetchType::kUtilityAudio:
        return kPrefetchArgument8;
      case AppLaunchPrefetchType::kUtilityStorage:
        return kPrefetchArgument8;
      case AppLaunchPrefetchType::kUtilityOther:
        return kPrefetchArgument8;
    }
  }
}

bool ChildProcessLauncherHelper::IsUsingLaunchOptions() {
  return true;
}

bool ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    FileMappedForLaunch& files_to_register,
    base::LaunchOptions* options) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  DCHECK_EQ(options->elevated, delegate_->ShouldLaunchElevated());
  if (!options->elevated) {
    mojo_channel_->PrepareToPassRemoteEndpoint(&options->handles_to_inherit,
                                               command_line());
  }
  return true;
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions* options,
    std::unique_ptr<FileMappedForLaunch> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  *is_synchronous_launch = true;
  if (delegate_->ShouldLaunchElevated()) {
    DCHECK(options->elevated);
    // When establishing a Mojo connection, the pipe path has already been added
    // to the command line.
    base::LaunchOptions win_options;
    win_options.start_hidden = true;
    win_options.elevated = true;
    ChildProcessLauncherHelper::Process process;
    process.process = base::LaunchProcess(*command_line(), win_options);
    *launch_result = process.process.IsValid() ? LAUNCH_RESULT_SUCCESS
                                               : LAUNCH_RESULT_FAILURE;
    return process;
  }
  ChildProcessLauncherHelper::Process process;
  *launch_result =
      StartSandboxedProcess(delegate_.get(), *command_line(),
                            options->handles_to_inherit, &process.process);
  return process;
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions* options) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
}

ChildProcessTerminationInfo ChildProcessLauncherHelper::GetTerminationInfo(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead) {
  ChildProcessTerminationInfo info;
  info.status =
      base::GetTerminationStatus(process.process.Handle(), &info.exit_code);
  return info;
}

// static
bool ChildProcessLauncherHelper::TerminateProcess(const base::Process& process,
                                                  int exit_code) {
  return process.Terminate(exit_code, false);
}

void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  // Client has gone away, so just kill the process.  Using exit code 0 means
  // that UMA won't treat this as a crash.
  process.process.Terminate(RESULT_CODE_NORMAL_EXIT, false);
}

void ChildProcessLauncherHelper::SetProcessPriorityOnLauncherThread(
    base::Process process,
    base::Process::Priority priority) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  if (process.CanSetPriority() && priority_ != priority) {
    priority_ = priority;
    process.SetPriority(priority);
  }
}

}  // namespace internal
}  // namespace content
