// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher_helper.h"

#include "base/base_switches.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "content/browser/child_process_launcher.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandbox_init_win.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "sandbox/win/src/sandbox_types.h"

namespace {

// Helper to avoid marking the log file as non-executable every time we launch a
// process.
bool ShouldMarkLogfileAsNonExecute() {
  static bool first_time = true;
  if (!first_time) {
    return false;
  }
  first_time = false;
  return true;
}

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
  // Windows uses LaunchOptions to pass filehandles to children.
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

void ChildProcessLauncherHelper::PassLoggingSwitches(
    base::LaunchOptions* launch_options,
    base::CommandLine* cmd_line) {
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  // Sandboxed processes on Windows cannot open files, and can't always figure
  // out default paths, so we directly pass a handle if logging is enabled.
  if (logging::IsLoggingToFileEnabled()) {
    // Make sure we're in charge of these flags.
    CHECK(!cmd_line->HasSwitch(switches::kEnableLogging));
    CHECK(!cmd_line->HasSwitch(switches::kLogFile));

    // Make best efforts attempt to mark the logfile as no-execute the first
    // time a process is started.
    if (ShouldMarkLogfileAsNonExecute()) {
      // Failure here means we pass in a writeable handle to a file that could
      // be marked executable and chained into a sandbox escape - but failure
      // should be rare and providing a logfile is already optional.
      std::ignore = base::PreventExecuteMappingUnchecked(
          base::FilePath(logging::GetLogFileFullPath()),
          base::PreventExecuteMappingClasses::GetPassKey());
    }

    log_handle_.Set(logging::DuplicateLogFileHandle());
    if (log_handle_.is_valid()) {
      // Override `--enable-logging --log-file=` switches so the child can log.
      cmd_line->AppendSwitchASCII(switches::kEnableLogging, "handle");
      auto handle_str =
          base::NumberToString(base::win::HandleToUint32(log_handle_.get()));
      cmd_line->AppendSwitchASCII(switches::kLogFile, handle_str);

      launch_options->handles_to_inherit.push_back(log_handle_.get());
    }
  }
#if !defined(OFFICIAL_BUILD)
  // Official builds do not send std handles to children so there is no point
  // in passing --enable-logging by itself. Debug builds might need to know if
  // stderr is being forced or not.
  else if (browser_command_line.HasSwitch(switches::kEnableLogging)) {
    std::string logging_destination =
        browser_command_line.GetSwitchValueASCII(switches::kEnableLogging);
    cmd_line->AppendSwitchASCII(switches::kEnableLogging, logging_destination);
  }
#endif
  // Forward other switches like other platforms.
  constexpr const char* kForwardSwitches[] = {
      switches::kDisableLogging,
      switches::kLoggingLevel,
      switches::kV,
      switches::kVModule,
  };
  cmd_line->CopySwitchesFrom(browser_command_line, kForwardSwitches);
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
