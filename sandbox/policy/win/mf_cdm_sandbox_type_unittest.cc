// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/sandbox_type.h"

#include "base/command_line.h"
#include "sandbox/policy/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(SandboxTypeTest, Utility) {
  // Setup to have '--type=utility' first.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(sandbox::policy::switches::kProcessType,
                                 sandbox::policy::switches::kUtilityProcess);
  EXPECT_EQ(sandbox::policy::SandboxType::kUtility,
            sandbox::policy::SandboxTypeFromCommandLine(command_line));

  base::CommandLine command_line2(command_line);
  SetCommandLineFlagsForSandboxType(
      &command_line2, sandbox::policy::SandboxType::kMediaFoundationCdm);
  EXPECT_EQ(sandbox::policy::SandboxType::kMediaFoundationCdm,
            sandbox::policy::SandboxTypeFromCommandLine(command_line2));
}

}  // namespace media
