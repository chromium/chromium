// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/sandbox_helper_win.h"

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "sandbox/win/src/sandbox_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(SandboxHelperWinTest, UnsandboxedChildHasNoServices) {
  base::test::ScopedCommandLine scoped_command_line;
  *scoped_command_line.GetProcessCommandLine() =
      base::CommandLine::FromString(L"test.exe --type=utility --no-sandbox");
  sandbox::SandboxInterfaceInfo sandbox_info = {};
  InitializeSandboxInfo(&sandbox_info);
  EXPECT_EQ(nullptr, sandbox_info.broker_services);
  EXPECT_EQ(nullptr, sandbox_info.target_services);
}

}  // namespace content
