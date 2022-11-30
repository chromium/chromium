// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/python_utils.h"

#include <memory>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "build/build_config.h"

namespace {
const base::FilePath::CharType kPythonPathEnv[] =
    FILE_PATH_LITERAL("PYTHONPATH");
const base::FilePath::CharType kVPythonClearPathEnv[] =
    FILE_PATH_LITERAL("VPYTHON_CLEAR_PYTHONPATH");
}  // namespace

void SetPythonPathInEnvironment(const std::vector<base::FilePath>& python_path,
                                base::EnvironmentMap* map) {
  base::NativeEnvironmentString path_str;
  for (const auto& path : python_path) {
    if (!path_str.empty()) {
#if BUILDFLAG(IS_WIN)
      path_str.push_back(';');
#else
      path_str.push_back(':');
#endif
    }
    path_str += path.value();
  }

  (*map)[kPythonPathEnv] = path_str;

  // vpython has instructions on BuildBot (not swarming or LUCI) to clear
  // PYTHONPATH on invocation. Since we are clearing and manipulating it
  // ourselves, we don't want vpython to throw out our hard work.
  (*map)[kVPythonClearPathEnv] = base::NativeEnvironmentString();
}

bool GetPython3Command(base::CommandLine* python_cmd) {
  DCHECK(python_cmd);

// Use vpython3 to pick up src.git's vpython3 VirtualEnv spec.
#if BUILDFLAG(IS_WIN)
  python_cmd->SetProgram(base::FilePath(FILE_PATH_LITERAL("vpython3.bat")));
#else
  python_cmd->SetProgram(base::FilePath(FILE_PATH_LITERAL("vpython3")));
#endif

#if BUILDFLAG(IS_MAC)
  // Enable logging to help diagnose https://crbug.com/1254962. Remove this when
  // the bug is resolved.
  python_cmd->AppendArg("-vpython-log-level=info");
#endif

  // Launch python in unbuffered mode, so that python output doesn't mix with
  // gtest output in buildbot log files. See http://crbug.com/147368.
  python_cmd->AppendArg("-u");

  return true;
}
