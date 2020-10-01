// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/sandbox_type.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "sandbox/policy/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace policy {

TEST(SandboxTypeTest, Empty) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  EXPECT_EQ(SandboxType::kNoSandbox, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitchASCII(switches::kServiceSandboxType, "network");
  EXPECT_EQ(SandboxType::kNoSandbox, SandboxTypeFromCommandLine(command_line));

#if defined(OS_WIN)
  EXPECT_FALSE(
      command_line.HasSwitch(switches::kNoSandboxAndElevatedPrivileges));
  SetCommandLineFlagsForSandboxType(
      &command_line, SandboxType::kNoSandboxAndElevatedPrivileges);
  EXPECT_EQ(SandboxType::kNoSandboxAndElevatedPrivileges,
            SandboxTypeFromCommandLine(command_line));
#endif

  EXPECT_FALSE(command_line.HasSwitch(switches::kNoSandbox));
  SetCommandLineFlagsForSandboxType(&command_line, SandboxType::kNoSandbox);
  EXPECT_EQ(SandboxType::kNoSandbox, SandboxTypeFromCommandLine(command_line));
  EXPECT_TRUE(command_line.HasSwitch(switches::kNoSandbox));
}

TEST(SandboxTypeTest, Renderer) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType,
                                 switches::kRendererProcess);
  EXPECT_EQ(SandboxType::kRenderer, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitchASCII(switches::kServiceSandboxType, "network");
  EXPECT_EQ(SandboxType::kRenderer, SandboxTypeFromCommandLine(command_line));

  EXPECT_FALSE(command_line.HasSwitch(switches::kNoSandbox));
  SetCommandLineFlagsForSandboxType(&command_line, SandboxType::kNoSandbox);
  EXPECT_EQ(SandboxType::kNoSandbox, SandboxTypeFromCommandLine(command_line));
  EXPECT_TRUE(command_line.HasSwitch(switches::kNoSandbox));
}

TEST(SandboxTypeTest, Utility) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType,
                                 switches::kUtilityProcess);
  EXPECT_EQ(SandboxType::kUtility, SandboxTypeFromCommandLine(command_line));

  base::CommandLine command_line2(command_line);
  SetCommandLineFlagsForSandboxType(&command_line2, SandboxType::kNetwork);
  EXPECT_EQ(SandboxType::kNetwork, SandboxTypeFromCommandLine(command_line2));

  base::CommandLine command_line3(command_line);
  SetCommandLineFlagsForSandboxType(&command_line3, SandboxType::kCdm);
  EXPECT_EQ(SandboxType::kCdm, SandboxTypeFromCommandLine(command_line3));

  base::CommandLine command_line4(command_line);
  SetCommandLineFlagsForSandboxType(&command_line4, SandboxType::kNoSandbox);
  EXPECT_EQ(SandboxType::kNoSandbox, SandboxTypeFromCommandLine(command_line4));

  base::CommandLine command_line5(command_line);
  SetCommandLineFlagsForSandboxType(&command_line5, SandboxType::kPpapi);
  EXPECT_EQ(SandboxType::kPpapi, SandboxTypeFromCommandLine(command_line5));

  base::CommandLine command_line6(command_line);
  command_line6.AppendSwitchASCII(switches::kServiceSandboxType, "bogus");
  EXPECT_EQ(SandboxType::kUtility, SandboxTypeFromCommandLine(command_line6));

  base::CommandLine command_line7(command_line);
  SetCommandLineFlagsForSandboxType(&command_line7,
                                    SandboxType::kPrintCompositor);
  EXPECT_EQ(SandboxType::kPrintCompositor,
            SandboxTypeFromCommandLine(command_line7));

  base::CommandLine command_line8(command_line);
  SetCommandLineFlagsForSandboxType(&command_line8, SandboxType::kAudio);
  EXPECT_EQ(SandboxType::kAudio, SandboxTypeFromCommandLine(command_line8));

  base::CommandLine command_line9(command_line);
  SetCommandLineFlagsForSandboxType(&command_line9,
                                    SandboxType::kSpeechRecognition);
  EXPECT_EQ(SandboxType::kSpeechRecognition,
            SandboxTypeFromCommandLine(command_line9));

#if defined(OS_WIN)
  base::CommandLine command_line10(command_line);
  SetCommandLineFlagsForSandboxType(&command_line10,
                                    SandboxType::kXrCompositing);
  EXPECT_EQ(SandboxType::kXrCompositing,
            SandboxTypeFromCommandLine(command_line10));

  base::CommandLine command_line11(command_line);
  SetCommandLineFlagsForSandboxType(&command_line11,
                                    SandboxType::kProxyResolver);
  EXPECT_EQ(SandboxType::kProxyResolver,
            SandboxTypeFromCommandLine(command_line11));

  base::CommandLine command_line12(command_line);
  SetCommandLineFlagsForSandboxType(&command_line12,
                                    SandboxType::kPdfConversion);
  EXPECT_EQ(SandboxType::kPdfConversion,
            SandboxTypeFromCommandLine(command_line12));
#endif

  base::CommandLine command_line13(command_line);
  command_line13.AppendSwitchASCII(switches::kServiceSandboxType,
                                   switches::kNoneSandbox);
  EXPECT_EQ(SandboxType::kNoSandbox,
            SandboxTypeFromCommandLine(command_line13));

  command_line.AppendSwitch(switches::kNoSandbox);
  EXPECT_EQ(SandboxType::kNoSandbox, SandboxTypeFromCommandLine(command_line));
}

TEST(SandboxTypeTest, GPU) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType, switches::kGpuProcess);
  SetCommandLineFlagsForSandboxType(&command_line, SandboxType::kGpu);
  EXPECT_EQ(SandboxType::kGpu, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitchASCII(switches::kServiceSandboxType, "network");
  EXPECT_EQ(SandboxType::kGpu, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitch(switches::kNoSandbox);
  EXPECT_EQ(SandboxType::kNoSandbox, SandboxTypeFromCommandLine(command_line));
}

TEST(SandboxTypeTest, PPAPIBroker) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType,
                                 switches::kPpapiBrokerProcess);
  EXPECT_EQ(SandboxType::kNoSandbox, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitchASCII(switches::kServiceSandboxType, "network");
  EXPECT_EQ(SandboxType::kNoSandbox, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitch(switches::kNoSandbox);
  EXPECT_EQ(SandboxType::kNoSandbox, SandboxTypeFromCommandLine(command_line));
}

TEST(SandboxTypeTest, PPAPIPlugin) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType,
                                 switches::kPpapiPluginProcess);
  SetCommandLineFlagsForSandboxType(&command_line, SandboxType::kPpapi);
  EXPECT_EQ(SandboxType::kPpapi, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitchASCII(switches::kServiceSandboxType, "network");
  EXPECT_EQ(SandboxType::kPpapi, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitch(switches::kNoSandbox);
  EXPECT_EQ(SandboxType::kNoSandbox, SandboxTypeFromCommandLine(command_line));
}

TEST(SandboxTypeTest, Nonesuch) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType, "nonesuch");
  // If tested here would CHECK.

  command_line.AppendSwitchASCII(switches::kServiceSandboxType, "network");
  // If tested here would CHECK.

  // With kNoSandbox will parse the command line correctly.
  command_line.AppendSwitch(switches::kNoSandbox);
  EXPECT_EQ(SandboxType::kNoSandbox, SandboxTypeFromCommandLine(command_line));
}

TEST(SandboxTypeTest, ElevatedPrivileges) {
  // Tests that the "no sandbox and elevated privileges" which is Windows
  // specific default to no sandbox on non Windows platforms.
  SandboxType elevated_type =
      UtilitySandboxTypeFromString(switches::kNoneSandboxAndElevatedPrivileges);
#if defined(OS_WIN)
  EXPECT_EQ(SandboxType::kNoSandboxAndElevatedPrivileges, elevated_type);
#else
  EXPECT_EQ(SandboxType::kNoSandbox, elevated_type);
#endif
}

}  // namespace policy
}  // namespace sandbox
