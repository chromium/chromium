// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/launch_native_messaging_host_process.h"

#include <unistd.h>

#include <string_view>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/process/launch.h"

namespace remoting {

ProcessLaunchResult LaunchNativeMessagingHostProcess(
    const base::FilePath& binary_path,
    intptr_t /* unused_parent_window_handle */,
    bool elevate_process,
    base::File& read_handle,
    base::File& write_handle) {
  base::ScopedFD parent_read_fd, child_stdout_fd;
  if (!base::CreatePipe(&parent_read_fd, &child_stdout_fd)) {
    PLOG(ERROR) << "Failed to create the read pipe.";
    return PROCESS_LAUNCH_RESULT_FAILED;
  }

  base::ScopedFD child_stdin_fd, parent_write_fd;
  if (!base::CreatePipe(&child_stdin_fd, &parent_write_fd)) {
    PLOG(ERROR) << "Failed to create the write pipe.";
    return PROCESS_LAUNCH_RESULT_FAILED;
  }

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  base::LaunchOptions options;

  if (elevate_process) {
    // `pkexec <binary_path>`
    command_line.SetProgram(base::FilePath("pkexec"));
    command_line.AppendArg(binary_path.value());
    // pkexec is a setuid binary, so we must allow new privileges.
    options.allow_new_privs = true;
  } else {
    command_line.SetProgram(binary_path);
  }

  options.fds_to_remap.emplace_back(child_stdin_fd.get(), STDIN_FILENO);
  options.fds_to_remap.emplace_back(child_stdout_fd.get(), STDOUT_FILENO);

  base::Process process = base::LaunchProcess(command_line, options);
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch " << (elevate_process ? "elevated " : "")
               << "native messaging host.";
    return PROCESS_LAUNCH_RESULT_FAILED;
  }

  read_handle = base::File(parent_read_fd.release());
  write_handle = base::File(parent_write_fd.release());

  // Child FDs in the parent process will be automatically released.

  return PROCESS_LAUNCH_RESULT_SUCCESS;
}

}  // namespace remoting
