// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <string_view>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/base/username.h"
#include "remoting/host/mac/constants_mac.h"
#include "remoting/host/version.h"

namespace remoting {
namespace {

constexpr char kSwitchDisable[] = "disable";
constexpr char kSwitchEnable[] = "enable";
constexpr char kSwitchSaveConfig[] = "save-config";
constexpr char kSwitchHostVersion[] = "host-version";
constexpr char kSwitchHostRunFromLaunchd[] = "run-from-launchd";

constexpr char kHostExeFileName[] = "remoting_me2me_host";
constexpr char kNativeMessagingHostPath[] =
    "Contents/MacOS/native_messaging_host";

// The exit code returned by 'wait' when a process is terminated by SIGTERM.
constexpr int kSigtermExitCode = 128 + SIGTERM;

// Constants controlling the host process relaunch throttling.
constexpr base::TimeDelta kMinimumRelaunchInterval = base::Minutes(1);
constexpr int kMaximumHostFailures = 10;

// Exit code 126 is defined by Posix to mean "Command found, but not
// executable", and is returned if the process cannot be launched due to
// parental control.
constexpr int kPermissionDeniedParentalControl = 126;

// This executable works as a proxy between launchd and the host. Signals of
// interest to the host must be forwarded.
constexpr int kSignalList[] = {
    SIGHUP,  SIGINT,    SIGQUIT, SIGILL,   SIGTRAP, SIGABRT, SIGEMT,
    SIGFPE,  SIGBUS,    SIGSEGV, SIGSYS,   SIGPIPE, SIGALRM, SIGTERM,
    SIGURG,  SIGTSTP,   SIGCONT, SIGTTIN,  SIGTTOU, SIGIO,   SIGXCPU,
    SIGXFSZ, SIGVTALRM, SIGPROF, SIGWINCH, SIGINFO, SIGUSR1, SIGUSR2};

// Current host PID used to forward signals. 0 if host is not running.
static base::ProcessId g_host_pid = 0;

void HandleSignal(int signum) {
  if (g_host_pid) {
    // All other signals are forwarded to host then ignored except SIGTERM.
    // launchd sends SIGTERM when service is being stopped so both the host and
    // the host service need to terminate.
    HOST_LOG << "Forwarding signal " << signum << " to host process "
             << g_host_pid;
    kill(g_host_pid, signum);
    if (signum == SIGTERM) {
      HOST_LOG << "Host service is terminating upon reception of SIGTERM";
      exit(kSigtermExitCode);
    }
  } else {
    HOST_LOG << "Signal " << signum
             << " will not be forwarded since host is not running.";
    exit(128 + signum);
  }
}

void RegisterSignalHandler() {
  struct sigaction action = {};
  sigfillset(&action.sa_mask);
  action.sa_flags = 0;
  action.sa_handler = &HandleSignal;

  for (int signum : kSignalList) {
    if (sigaction(signum, &action, nullptr) == -1) {
      PLOG(DFATAL) << "Failed to register signal handler for signal " << signum;
    }
  }
}

class HostService {
 public:
  HostService();
  ~HostService();

  bool Disable();
  bool Enable();
  bool WriteStdinToConfig();
  int RunHost();
  void PrintHostVersion();
  void PrintPid();

 private:
  int RunHostFromOldScript();

  // Runs the permission-checker built into the native-messaging host. Returns
  // true if all permissions were granted (or no permission check is needed for
  // the version of MacOS).
  bool CheckPermission();

  bool HostIsEnabled();

  base::FilePath old_host_helper_file_;
  base::FilePath enabled_file_;
  base::FilePath config_file_;
  base::FilePath host_exe_file_;
  base::FilePath native_messaging_host_exe_file_;
};

HostService::HostService() {
  old_host_helper_file_ = base::FilePath(kOldHostHelperScriptPath);
  enabled_file_ = base::FilePath(kHostEnabledPath);
  config_file_ = base::FilePath(kHostConfigFilePath);

  base::FilePath host_service_dir;
  base::PathService::Get(base::DIR_EXE, &host_service_dir);
  host_exe_file_ = host_service_dir.AppendASCII(kHostExeFileName);
  native_messaging_host_exe_file_ =
      host_service_dir.AppendASCII(NATIVE_MESSAGING_HOST_BUNDLE_NAME)
          .AppendASCII(kNativeMessagingHostPath);
}

HostService::~HostService() = default;

int HostService::RunHost() {
  if (geteuid() != 0 && HostIsEnabled()) {
    // Only check for non-root users, as the permission wizard is not actionable
    // at the login screen. Also, permission is only needed when host is
    // enabled - the launchd service should exit immediately if the host is
    // disabled.
    if (!CheckPermission()) {
      return 1;
    }
  }

  int host_failure_count = 0;
  base::TimeTicks host_start_time;

  while (true) {
    if (!HostIsEnabled()) {
      HOST_LOG << "Daemon is disabled.";
      return 0;
    }

    // If this is not the first time the host has run, make sure we don't
    // relaunch it too soon.
    if (!host_start_time.is_null()) {
      base::TimeDelta host_lifetime = base::TimeTicks::Now() - host_start_time;
      HOST_LOG << "Host ran for " << host_lifetime;
      if (host_lifetime < kMinimumRelaunchInterval) {
        // If the host didn't run for very long, assume it crashed. Relaunch
        // only after a suitable delay and increase the failure count.
        host_failure_count++;
        LOG(WARNING) << "Host failure count " << host_failure_count << "/"
                     << kMaximumHostFailures;
        if (host_failure_count >= kMaximumHostFailures) {
          LOG(ERROR) << "Too many host failures. Giving up.";
          return 1;
        }
        // TODO: crbug.com/366071356 - use exponential backoff
        base::TimeDelta relaunch_in = kMinimumRelaunchInterval - host_lifetime;
        HOST_LOG << "Relaunching in " << relaunch_in;
        base::PlatformThread::Sleep(relaunch_in);
      } else {
        // If the host ran for long enough, reset the crash counter.
        host_failure_count = 0;
      }
    }

    host_start_time = base::TimeTicks::Now();
    base::CommandLine cmdline(host_exe_file_);
    cmdline.AppendSwitchPath("host-config", config_file_);
    std::string ssh_auth_sockname =
        "/tmp/chromoting." + GetUsername() + ".ssh_auth_sock";
    cmdline.AppendSwitchASCII("ssh-auth-sockname", ssh_auth_sockname);
    base::Process process = base::LaunchProcess(cmdline, base::LaunchOptions());
    if (!process.IsValid()) {
      LOG(ERROR) << "Failed to launch host process for unknown reason.";
      return 1;
    }

    g_host_pid = process.Pid();
    int exit_code;
    process.WaitForExit(&exit_code);
    g_host_pid = 0;
    const char* exit_code_string_ptr = ExitCodeToStringUnchecked(exit_code);
    std::string exit_code_string =
        exit_code_string_ptr ? (std::string(exit_code_string_ptr) + " (" +
                                base::NumberToString(exit_code) + ")")
                             : base::NumberToString(exit_code);

    if (exit_code == 0 || exit_code == kSigtermExitCode ||
        exit_code == kPermissionDeniedParentalControl ||
        (exit_code >= kMinPermanentErrorExitCode &&
         exit_code <= kMaxPermanentErrorExitCode)) {
      HOST_LOG << "Host returned permanent exit code " << exit_code_string
               << " at " << base::Time::Now();
      if (exit_code == kInvalidHostIdExitCode ||
          exit_code == kHostDeletedExitCode) {
        // The host was taken off-line remotely. To prevent the host being
        // restarted when the login context changes, try to delete the "enabled"
        // file. Since this requires root privileges, this is only possible when
        // this executable is launched in the "login" context. In the "aqua"
        // context, just exit and try again next time.
        HOST_LOG << "Host deleted - disabling";
        Disable();
      }
      return exit_code;
    }

    // Ignore non-permanent error-code and launch host again. Stop handling
    // signals temporarily in case the executable has to sleep to throttle host
    // relaunches. While throttling, there is no host process to which to
    // forward the signal, so the default behaviour should be restored.
    HOST_LOG << "Host returned non-permanent exit code " << exit_code_string
             << " at " << base::Time::Now();
  }
}

bool HostService::Disable() {
  return base::DeleteFile(enabled_file_);
}

bool HostService::Enable() {
  // Ensure the config file is private whilst being written.
  base::DeleteFile(config_file_);
  if (!WriteStdinToConfig()) {
    return false;
  }
  if (!base::SetPosixFilePermissions(config_file_, 0600)) {
    LOG(ERROR) << "Failed to set posix permission";
    return false;
  }

  // Ensure the config is readable by the user registering the host.
  // We don't seem to have API for adding Mac ACL entry for file. This code just
  // uses the chmod binary to do so.
  base::CommandLine chmod_cmd(base::FilePath("/bin/chmod"));
  chmod_cmd.AppendArg("+a");
  chmod_cmd.AppendArg("user:" + GetUsername() + ":allow:read");
  chmod_cmd.AppendArgPath(config_file_);
  std::string output;
  if (!base::GetAppOutputAndError(chmod_cmd, &output)) {
    LOG(ERROR) << "Failed to chmod file " << config_file_;
    return false;
  }
  if (!output.empty()) {
    HOST_LOG << "Message from chmod: " << output;
  }

  if (!base::WriteFile(enabled_file_, std::string_view())) {
    LOG(ERROR) << "Failed to write enabled file";
    return false;
  }
  return true;
}

bool HostService::WriteStdinToConfig() {
  // Reads from stdin and writes it to the config file.
  std::istreambuf_iterator<char> begin(std::cin);
  std::istreambuf_iterator<char> end;
  std::string config(begin, end);
  if (!base::WriteFile(config_file_, config)) {
    LOG(ERROR) << "Failed to write config file";
    return false;
  }
  return true;
}

void HostService::PrintHostVersion() {
  printf("%s\n", STRINGIZE(VERSION));
}

void HostService::PrintPid() {
  // Caller running host service with privilege waits for the PID to continue,
  // so we need to flush it immediately.
  printf("%d\n", base::Process::Current().Pid());
  fflush(stdout);
}

int HostService::RunHostFromOldScript() {
  base::CommandLine cmdline(old_host_helper_file_);
  cmdline.AppendSwitch(kSwitchHostRunFromLaunchd);
  base::LaunchOptions options;
  options.disclaim_responsibility = true;
  base::Process process = base::LaunchProcess(cmdline, options);
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch the old host script for unknown reason.";
    return 1;
  }

  g_host_pid = process.Pid();
  int exit_code;
  process.WaitForExit(&exit_code);
  g_host_pid = 0;
  return exit_code;
}

bool HostService::CheckPermission() {
  LOG(INFO) << "Checking for host permissions.";

  base::CommandLine cmdLine(native_messaging_host_exe_file_);
  cmdLine.AppendSwitch(kCheckPermissionSwitchName);

  // No need to disclaim responsibility here - the native-messaging host already
  // takes care of that.
  base::Process process = base::LaunchProcess(cmdLine, base::LaunchOptions());
  if (!process.IsValid()) {
    LOG(ERROR) << "Unable to launch native-messaging host process";
    return false;
  }
  int exit_code;
  process.WaitForExit(&exit_code);
  if (exit_code != 0) {
    LOG(ERROR) << "A required permission was not granted.";
    return false;
  }
  LOG(INFO) << "All permissions granted!";
  return true;
}

bool HostService::HostIsEnabled() {
  return base::PathExists(enabled_file_);
}

}  // namespace
}  // namespace remoting

int main(int argc, char const* argv[]) {
  base::AtExitManager exitManager;
  base::CommandLine::Init(argc, argv);
  remoting::InitHostLogging();

  remoting::HostService service;
  auto* current_cmdline = base::CommandLine::ForCurrentProcess();
  std::string pid = base::NumberToString(base::Process::Current().Pid());
  if (current_cmdline->HasSwitch(remoting::kSwitchDisable)) {
    service.PrintPid();
    if (!service.Disable()) {
      LOG(ERROR) << "Failed to disable";
      return 1;
    }
  } else if (current_cmdline->HasSwitch(remoting::kSwitchEnable)) {
    service.PrintPid();
    if (!service.Enable()) {
      LOG(ERROR) << "Failed to enable";
      return 1;
    }
  } else if (current_cmdline->HasSwitch(remoting::kSwitchSaveConfig)) {
    service.PrintPid();
    if (!service.WriteStdinToConfig()) {
      LOG(ERROR) << "Failed to save config";
      return 1;
    }
  } else if (current_cmdline->HasSwitch(remoting::kSwitchHostVersion)) {
    service.PrintHostVersion();
  } else if (current_cmdline->HasSwitch(remoting::kSwitchHostRunFromLaunchd)) {
    remoting::RegisterSignalHandler();
    HOST_LOG << "Host started for user " << remoting::GetUsername() << " at "
             << base::Time::Now();
    return service.RunHost();
  } else {
    service.PrintPid();
    return 1;
  }
  return 0;
}
