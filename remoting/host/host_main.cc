// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the common entry point shared by all Chromoting Host
// processes.

#include "remoting/host/host_main.h"

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "remoting/base/breakpad.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/host_exit_codes.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/evaluate_capability.h"
#include "remoting/host/resources.h"
#include "remoting/host/setup/me2me_native_messaging_host.h"
#include "remoting/host/usage_stats_consent.h"

#if BUILDFLAG(IS_APPLE)
#include "base/apple/scoped_nsautorelease_pool.h"
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <commctrl.h>
#include <shellapi.h>
#endif  // BUILDFLAG(IS_WIN)

namespace remoting {

// Known entry points.
int HostProcessMain();
#if BUILDFLAG(IS_WIN)
int DaemonProcessMain();
int DesktopProcessMain();
int FileChooserMain();
int RdpDesktopSessionMain();
int UrlForwarderConfiguratorMain();
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
int XSessionChooserMain();
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace {

typedef int (*MainRoutineFn)();

const char kUsageMessage[] =
    "Usage: %s [options]\n"
    "\n"
    "Options:\n"

#if BUILDFLAG(IS_LINUX)
    "  --audio-pipe-name=<pipe> - Sets the pipe name to capture audio on "
    "Linux.\n"
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_APPLE)
    "  --list-audio-devices     - List all audio devices and their device "
    "UID.\n"
#endif  // BUILDFLAG(IS_APPLE)

    "  --console                - Runs the daemon interactively.\n"
    "  --elevate=<binary>       - Runs <binary> elevated.\n"
    "  --host-config=<config>   - Specifies the host configuration.\n"
    "  --help, -?               - Prints this message.\n"
    "  --type                   - Specifies process type.\n"
    "  --version                - Prints the host version and exits.\n"
    "  --evaluate-type=<type>   - Evaluates the capability of the host.\n"
    "  --enable-utempter        - Enables recording to utmp/wtmp on Linux.\n"
    "  --webrtc-trace-event-file=<path> - Enables logging webrtc trace events "
    "to a file.\n";

void Usage(const base::FilePath& program_name) {
  printf(kUsageMessage, program_name.MaybeAsASCII().c_str());
}

#if BUILDFLAG(IS_WIN)

// Runs the binary specified by the command line, elevated.
int RunElevated() {
  const base::CommandLine::SwitchMap& switches =
      base::CommandLine::ForCurrentProcess()->GetSwitches();
  base::CommandLine::StringVector args =
      base::CommandLine::ForCurrentProcess()->GetArgs();

  // Create the child process command line by copying switches from the current
  // command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  for (base::CommandLine::SwitchMap::const_iterator i = switches.begin();
       i != switches.end(); ++i) {
    if (i->first != kElevateSwitchName) {
      command_line.AppendSwitchNative(i->first, i->second);
    }
  }
  for (base::CommandLine::StringVector::const_iterator i = args.begin();
       i != args.end(); ++i) {
    command_line.AppendArgNative(*i);
  }

  // Get the name of the binary to launch.
  base::FilePath binary =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kElevateSwitchName);
  base::CommandLine::StringType parameters =
      command_line.GetCommandLineString();

  // Launch the child process requesting elevation.
  SHELLEXECUTEINFO info;
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  info.lpVerb = L"runas";
  info.lpFile = binary.value().c_str();
  info.lpParameters = parameters.c_str();
  info.nShow = SW_SHOWNORMAL;

  if (!ShellExecuteEx(&info)) {
    DWORD exit_code = GetLastError();
    PLOG(ERROR) << "Unable to launch '" << binary.value() << "'";
    return exit_code;
  }

  return kSuccessExitCode;
}

#endif  // !BUILDFLAG(IS_WIN)

// Select the entry point corresponding to the process type.
MainRoutineFn SelectMainRoutine(const std::string& process_type) {
  MainRoutineFn main_routine = nullptr;

  if (process_type == kProcessTypeHost) {
    main_routine = &HostProcessMain;
#if BUILDFLAG(IS_WIN)
  } else if (process_type == kProcessTypeDaemon) {
    main_routine = &DaemonProcessMain;
  } else if (process_type == kProcessTypeDesktop) {
    main_routine = &DesktopProcessMain;
  } else if (process_type == kProcessTypeFileChooser) {
    main_routine = &FileChooserMain;
  } else if (process_type == kProcessTypeRdpDesktopSession) {
    main_routine = &RdpDesktopSessionMain;
  } else if (process_type == kProcessTypeUrlForwarderConfigurator) {
    main_routine = &UrlForwarderConfiguratorMain;
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  } else if (process_type == kProcessTypeXSessionChooser) {
    main_routine = &XSessionChooserMain;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  }

  return main_routine;
}

}  // namespace

int HostMain(int argc, char** argv) {
#if BUILDFLAG(IS_APPLE)
  // Needed so we don't leak objects when threads are created.
  base::apple::ScopedNSAutoreleasePool pool;
#endif

  base::CommandLine::Init(argc, argv);

  // Parse the command line.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kHelpSwitchName) ||
      command_line->HasSwitch(kQuestionSwitchName)) {
    Usage(command_line->GetProgram());
    return kSuccessExitCode;
  }

  if (command_line->HasSwitch(kVersionSwitchName)) {
    printf("%s\n", STRINGIZE(VERSION));
    return kSuccessExitCode;
  }

#if BUILDFLAG(IS_WIN)
  if (command_line->HasSwitch(kElevateSwitchName)) {
    return RunElevated();
  }
#endif  // BUILDFLAG(IS_WIN)

  // Assume the host process by default.
  std::string process_type = kProcessTypeHost;
  if (command_line->HasSwitch(kProcessTypeSwitchName)) {
    process_type = command_line->GetSwitchValueASCII(kProcessTypeSwitchName);
  }

  if (process_type == kProcessTypeEvaluateCapability) {
    if (command_line->HasSwitch(kEvaluateCapabilitySwitchName)) {
      return EvaluateCapabilityLocally(
          command_line->GetSwitchValueASCII(kEvaluateCapabilitySwitchName));
    }
    Usage(command_line->GetProgram());
    return kSuccessExitCode;
  }

  // This object instance is required by Chrome code (for example,
  // LazyInstance, MessageLoop).
  base::AtExitManager exit_manager;

  // Enable debug logs.
  InitHostLogging();

#if defined(REMOTING_ENABLE_BREAKPAD)
  // Initialize Breakpad as early as possible. On Mac the command-line needs to
  // be initialized first, so that the preference for crash-reporting can be
  // looked up in the config file.
  if (IsUsageStatsAllowed()) {
#if BUILDFLAG(IS_LINUX)
    InitializeCrashReporting();
#elif BUILDFLAG(IS_WIN)
    // TODO: joedow - Enable crash reporting for the RDP process.
    if (process_type == kProcessTypeDesktop ||
        process_type == kProcessTypeDaemon) {
      InitializeCrashReporting();
    } else if (command_line->HasSwitch(kCrashServerPipeHandle)) {
      InitializeOopCrashClient(
          command_line->GetSwitchValueASCII(kCrashServerPipeHandle));
    }
#endif
  }
#endif  // defined(REMOTING_ENABLE_BREAKPAD)

#if BUILDFLAG(IS_WIN)
  // Register and initialize common controls.
  INITCOMMONCONTROLSEX info;
  info.dwSize = sizeof(info);
  info.dwICC = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&info);
#endif  // BUILDFLAG(IS_WIN)

  MainRoutineFn main_routine = SelectMainRoutine(process_type);
  if (!main_routine) {
    fprintf(stderr, "Unknown process type '%s' specified.",
            process_type.c_str());
    Usage(command_line->GetProgram());
    return kInvalidCommandLineExitCode;
  }

  // Required to find the ICU data file, used by some file_util routines.
  base::i18n::InitializeICU();

  remoting::LoadResources("");

  mojo::core::Init({
#if BUILDFLAG(IS_WIN)
      .is_broker_process = main_routine == &DaemonProcessMain
#elif BUILDFLAG(IS_MAC)
      // The broker process on Mac is the agent process broker.
      .is_broker_process = false
#else
      .is_broker_process = main_routine == &HostProcessMain
#endif
  });

  // Invoke the entry point.
  int exit_code = main_routine();
  if (exit_code == kInvalidCommandLineExitCode) {
    Usage(command_line->GetProgram());
  }

  remoting::UnloadResources();

  return exit_code;
}

}  // namespace remoting
