// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_test_launcher_delegate.h"

#include "base/command_line.h"
#include "base/test/test_suite.h"
#include "content/shell/common/shell_switches.h"
#include "extensions/shell/test/test_shell_main_delegate.h"

namespace extensions {

int AppShellTestLauncherDelegate::RunTestSuite(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
  // Browser tests are expected not to tear-down various globals and may
  // complete with the thread priority being above NORMAL.
  test_suite.DisableCheckForLeakedGlobals();
  test_suite.DisableCheckForThreadPriorityAtTestEnd();
  return test_suite.Run();
}

bool AppShellTestLauncherDelegate::AdjustChildProcessCommandLine(
    base::CommandLine* command_line,
    const base::FilePath& temp_data_dir) {
  command_line->AppendSwitchPath(switches::kContentShellDataPath,
                                 temp_data_dir);
  return true;
}

content::ContentMainDelegate*
AppShellTestLauncherDelegate::CreateContentMainDelegate() {
  return new TestShellMainDelegate();
}

}  // namespace extensions
