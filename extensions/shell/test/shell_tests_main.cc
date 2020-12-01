// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/launcher/test_launcher.h"
#include "build/build_config.h"
#include "extensions/shell/test/shell_test_launcher_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif  // defined(OS_WIN)

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  size_t parallel_jobs = base::NumParallelJobs(/*cores_per_job=*/2);
  if (parallel_jobs == 0U)
    return 1;

#if defined(OS_WIN)
  // Load and pin user32.dll to avoid having to load it once tests start while
  // on the main thread loop where blocking calls are disallowed.
  base::win::PinUser32();
#endif  // OS_WIN

  extensions::AppShellTestLauncherDelegate launcher_delegate;
  return content::LaunchTests(&launcher_delegate, parallel_jobs, argc, argv);
}
