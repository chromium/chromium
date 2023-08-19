// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/sandbox_type.h"

#include "base/command_line.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(MfCdmSandboxTypeTest, Utility) {
  // Setup to have '--type=utility' first (but no valid sandbox).
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(sandbox::policy::switches::kProcessType,
                                 sandbox::policy::switches::kUtilityProcess);

  base::CommandLine command_line2(command_line);
  sandbox::policy::SetCommandLineFlagsForSandboxType(
      &command_line2, sandbox::mojom::Sandbox::kMediaFoundationCdm);
  EXPECT_EQ(sandbox::mojom::Sandbox::kMediaFoundationCdm,
            sandbox::policy::SandboxTypeFromCommandLine(command_line2));
}

}  // namespace media
