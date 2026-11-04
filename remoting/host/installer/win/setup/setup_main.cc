// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "remoting/base/logging.h"
#include "remoting/host/installer/win/setup/setup_runner.h"

int WINAPI wWinMain(HINSTANCE instance,
                    HINSTANCE prev_instance,
                    LPWSTR command_line,
                    int show_command) {
#if !defined(COMPONENT_BUILD)
  CHECK(::SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32));
#endif

  base::AtExitManager exit_manager;
  base::CommandLine::Init(0, nullptr);
  remoting::InitHostLogging();

  remoting::installer::SetupExitCode exit_code =
      remoting::installer::RunSetup(*base::CommandLine::ForCurrentProcess());
  return static_cast<int>(exit_code);
}
