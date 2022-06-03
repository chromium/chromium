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
  // Browser tests are expected not to tear-down various globals.
  test_suite.DisableCheckForLeakedGlobals();
  return test_suite.Run();
}

std::string
AppShellTestLauncherDelegate::GetUserDataDirectoryCommandLineSwitch() {
  return switches::kContentShellDataPath;
}

content::ContentMainDelegate*
AppShellTestLauncherDelegate::CreateContentMainDelegate() {
  return new TestShellMainDelegate();
}

}  // namespace extensions
