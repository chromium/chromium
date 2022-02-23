// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojo_ipc/mojo_caller_security_checker.h"

#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace remoting {
namespace {

constexpr auto kAllowedCallerProgramNames =
    base::MakeFixedFlatSet<base::StringPiece>({
        "remote-open-url",
        "remote-webauthn",
    });

base::FilePath GetProcessImagePath(base::ProcessId pid) {
  // We don't get the image path from the command line, since it's spoofable by
  // the process itself.
  base::FilePath process_exe_path(
      base::StringPrintf("/proc/%" CrPRIdPid "/exe", pid));
  base::FilePath process_image_path;
  base::ReadSymbolicLink(process_exe_path, &process_image_path);
  return process_image_path;
}

}  // namespace

bool IsTrustedMojoEndpoint(base::ProcessId caller_pid) {
  static base::NoDestructor<base::FilePath> current_process_image_path(
      GetProcessImagePath(base::GetCurrentProcId()));
  base::FilePath caller_process_image_path = GetProcessImagePath(caller_pid);
  if (caller_process_image_path.empty()) {
    LOG(ERROR) << "Cannot resolve process image path for PID " << caller_pid;
    return false;
  }
  if (caller_process_image_path == *current_process_image_path) {
    // IPCs initiated from the same binary should be allowed.
    return true;
  }
  if (caller_process_image_path.DirName() !=
      current_process_image_path->DirName()) {
    LOG(ERROR) << "Process image " << caller_process_image_path
               << " is not under " << current_process_image_path->DirName();
    return false;
  }
  std::string program_name = caller_process_image_path.BaseName().value();
  if (!kAllowedCallerProgramNames.contains(program_name)) {
#if !defined(NDEBUG)
    // Binaries generated in out/Debug are underscore-separated. To make
    // debugging easier, we just check the name again with underscores replaced
    // with hyphens.
    std::string program_name_hyphenated;
    base::ReplaceChars(program_name, "_", "-", &program_name_hyphenated);
    if (kAllowedCallerProgramNames.contains(program_name_hyphenated)) {
      return true;
    }
#endif  // !defined(NDEBUG)
    LOG(ERROR) << caller_process_image_path.BaseName()
               << " is not in the list of allowed caller programs.";
    return false;
  }
  return true;
}

}  // namespace remoting
