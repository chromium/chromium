// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_TEST_SHELL_TEST_LAUNCHER_DELEGATE_H_
#define EXTENSIONS_SHELL_TEST_SHELL_TEST_LAUNCHER_DELEGATE_H_

#include "content/public/test/test_launcher.h"

namespace extensions {

class AppShellTestLauncherDelegate : public content::TestLauncherDelegate {
 public:
  int RunTestSuite(int argc, char** argv) override;
  std::string GetUserDataDirectoryCommandLineSwitch() override;
  content::ContentMainDelegate* CreateContentMainDelegate() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_TEST_SHELL_TEST_LAUNCHER_DELEGATE_H_
