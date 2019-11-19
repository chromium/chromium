// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mac/permission_process_utils.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "remoting/host/mac/constants_mac.h"
#include "remoting/host/version.h"

namespace remoting {
namespace mac {

namespace {

base::FilePath GetHostExePath(HostMode mode) {
  // Path to the host bundle top directory.
  base::FilePath host_path(kHostBinaryPath);

  host_path = host_path.Append("Contents/MacOS");
  if (mode == HostMode::ME2ME)
    return host_path.Append("remoting_me2me_host");

  return host_path.Append(REMOTE_ASSISTANCE_HOST_BUNDLE_NAME)
      .Append("Contents/MacOS/remote_assistance_host");
}

bool CheckHostPermission(base::FilePath exe_path, std::string command_switch) {
  base::CommandLine cmdLine(exe_path);
  cmdLine.AppendSwitch(command_switch);

  base::LaunchOptions options;
  options.disclaim_responsibility = true;
  base::Process process = base::LaunchProcess(cmdLine, options);
  if (!process.IsValid()) {
    LOG(ERROR) << "Unable to launch host process";
    return false;
  }
  int exit_code;
  process.WaitForExit(&exit_code);
  LOG(INFO) << "Permission '" << command_switch << "' is "
            << ((exit_code == 0) ? "granted" : "denied");
  return exit_code == 0;
}

}  // namespace

bool CheckAccessibilityPermission(HostMode mode) {
  return CheckHostPermission(GetHostExePath(mode),
                             "check-accessibility-permission");
}

bool CheckScreenRecordingPermission(HostMode mode) {
  return CheckHostPermission(GetHostExePath(mode),
                             "check-screen-recording-permission");
}

}  // namespace mac
}  // namespace remoting
