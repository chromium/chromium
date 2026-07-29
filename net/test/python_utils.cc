// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/python_utils.h"

#include <memory>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "build/build_config.h"

bool GetPython3Command(base::CommandLine* python_cmd) {
  DCHECK(python_cmd);

// Use vpython3 to pick up src.git's vpython3 VirtualEnv spec.
#if BUILDFLAG(IS_WIN)
  python_cmd->SetProgram(base::FilePath(FILE_PATH_LITERAL("vpython3.bat")));
#else
  python_cmd->SetProgram(base::FilePath(FILE_PATH_LITERAL("vpython3")));
#endif

  // Launch python in unbuffered mode, so that python output doesn't mix with
  // gtest output in buildbot log files. See http://crbug.com/147368.
  python_cmd->AppendArg("-u");

  return true;
}
