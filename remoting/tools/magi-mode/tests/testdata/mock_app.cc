// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mock application that uses the flawed test files.

#include "bind_post_task_helper.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "thread_safe_manager.h"
#include "type_converter.h"

#if BUILDFLAG(IS_WIN)
#include "file_manager_win.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "socket_handler_linux.h"
#endif

namespace remoting {

void RunMockApp() {
  // Use thread manager
  ResourceManager manager;
  manager.AcquireResources();
  manager.ReleaseResources();

  // Use type converter
  Base* base = new DerivedA();
  ConvertType(base);
  delete base;

  // Use callback helper
  BindPostTaskHelper helper;
  helper.ScheduleWork();

#if BUILDFLAG(IS_WIN)
  ProcessFile();
#endif

#if BUILDFLAG(IS_LINUX)
  ReadConfig();
#endif
}

}  // namespace remoting

int main() {
  remoting::RunMockApp();
  return 0;
}
