// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/command_line.h"
#include "remoting/base/logging.h"
#include "remoting/host/installer/win/mini_installer/mini_installer.h"

#if defined(COMPONENT_BUILD)
static_assert(false,
              "remoting_mini_installer is not supported in component builds.");
#endif

int WINAPI wWinMain(HINSTANCE instance,
                    HINSTANCE prev_instance,
                    LPWSTR command_line,
                    int show_command) {
  CHECK(::SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32));

  base::AtExitManager exit_manager;
  base::CommandLine::Init(0, nullptr);
  remoting::InitHostLogging();

  return remoting::installer::RunMiniInstaller(
      instance, *base::CommandLine::ForCurrentProcess());
}
