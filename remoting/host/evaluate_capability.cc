// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/evaluate_capability.h"

#include <iostream>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "build/build_config.h"
#include "remoting/host/host_exit_codes.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/switches.h"

#if defined(OS_WIN)
#include "remoting/host/win/evaluate_3d_display_mode.h"
#include "remoting/host/win/evaluate_d3d.h"
#endif

namespace remoting {

namespace {

// Returns the full path of the binary file we should use to evaluate the
// capability. According to the platform and executing environment, return of
// this function may vary. But in one process, the return value is guaranteed to
// be the same.
// This function uses capability_test_stub in unittest, or tries to use current
// binary if supported, otherwise it falls back to use the default binary.
base::FilePath BuildHostBinaryPath() {
  base::FilePath path;
  bool result = base::PathService::Get(base::FILE_EXE, &path);
  DCHECK(result);
  base::FilePath directory;
  result = base::PathService::Get(base::DIR_EXE, &directory);
  DCHECK(result);
#if defined(OS_WIN)
  if (path.BaseName().value() == FILE_PATH_LITERAL("remoting_unittests.exe")) {
    return directory.Append(FILE_PATH_LITERAL("capability_test_stub.exe"));
  }
#else
  if (path.BaseName().value() == FILE_PATH_LITERAL("remoting_unittests")) {
    return directory.Append(FILE_PATH_LITERAL("capability_test_stub"));
  }
#endif

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  if (path.BaseName().value() ==
      FILE_PATH_LITERAL("chrome-remote-desktop-host")) {
    return path;
  }
  if (path.BaseName().value() == FILE_PATH_LITERAL("remoting_me2me_host")) {
    return path;
  }

  return directory.Append(FILE_PATH_LITERAL("remoting_me2me_host"));
#elif defined(OS_APPLE)
  if (path.BaseName().value() == FILE_PATH_LITERAL("remoting_me2me_host")) {
    return path;
  }

  return directory.Append(FILE_PATH_LITERAL(
      "remoting_me2me_host.app/Contents/MacOS/remoting_me2me_host"));
#elif defined(OS_WIN)
  if (path.BaseName().value() == FILE_PATH_LITERAL("remoting_console.exe")) {
    return path;
  }
  if (path.BaseName().value() == FILE_PATH_LITERAL("remoting_desktop.exe")) {
    return path;
  }
  if (path.BaseName().value() == FILE_PATH_LITERAL("remoting_host.exe")) {
    return path;
  }

  return directory.Append(FILE_PATH_LITERAL("remoting_host.exe"));
#else
  #error "BuildHostBinaryPath is not implemented for current platform."
#endif
}

}  // namespace

int EvaluateCapabilityLocally(const std::string& type) {
#if defined(OS_WIN)
  if (type == kEvaluateD3D) {
    return EvaluateD3D();
  }
  if (type == kEvaluate3dDisplayMode) {
    return Evaluate3dDisplayMode();
  }
#endif

  return kInvalidCommandLineExitCode;
}

int EvaluateCapability(const std::string& type,
                       std::string* output /* = nullptr */) {
  base::CommandLine command(BuildHostBinaryPath());
  command.AppendSwitchASCII(kProcessTypeSwitchName,
                            kProcessTypeEvaluateCapability);
  command.AppendSwitchASCII(kEvaluateCapabilitySwitchName, type);

  int exit_code;
  std::string dummy_output;
  if (!output) {
    output = &dummy_output;
  }

  bool result = base::GetAppOutputWithExitCode(command, output, &exit_code);
#if defined(OS_WIN)
  // On Windows, base::GetAppOutputWithExitCode() usually returns false when
  // receiving "unknown" exit code. See
  // https://cs.chromium.org/chromium/src/base/process/launch_win.cc?rcl=39ec40095376e8d977decbdc5d7ca28ba7d39cf2&l=130
  // But we forward the |exit_code| through return value, so the return value of
  // base::GetAppOutputWithExitCode() should be ignored.
  result = true;
#endif
  if (!result) {
    LOG(ERROR) << "Failed to execute process "
               << command.GetCommandLineString()
               << ", exit code "
               << exit_code;
    // This should not happen.
    NOTREACHED();
  }

  return exit_code;
}

}  // namespace remoting
