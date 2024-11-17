// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/sandbox_type.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {
namespace policy {
using sandbox::mojom::Sandbox;

TEST(SandboxTypeTest, Empty) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  EXPECT_EQ(Sandbox::kNoSandbox, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitchASCII(switches::kServiceSandboxType, "network");
  EXPECT_EQ(Sandbox::kNoSandbox, SandboxTypeFromCommandLine(command_line));

  EXPECT_FALSE(command_line.HasSwitch(switches::kNoSandbox));
  SetCommandLineFlagsForSandboxType(&command_line, Sandbox::kNoSandbox);
  EXPECT_EQ(Sandbox::kNoSandbox, SandboxTypeFromCommandLine(command_line));
  EXPECT_TRUE(command_line.HasSwitch(switches::kNoSandbox));
}

TEST(SandboxTypeTest, Renderer) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType,
                                 switches::kRendererProcess);
  EXPECT_EQ(Sandbox::kRenderer, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitchASCII(switches::kServiceSandboxType, "network");
  EXPECT_EQ(Sandbox::kRenderer, SandboxTypeFromCommandLine(command_line));

  EXPECT_FALSE(command_line.HasSwitch(switches::kNoSandbox));
  SetCommandLineFlagsForSandboxType(&command_line, Sandbox::kNoSandbox);
  EXPECT_EQ(Sandbox::kNoSandbox, SandboxTypeFromCommandLine(command_line));
  EXPECT_TRUE(command_line.HasSwitch(switches::kNoSandbox));
}

TEST(SandboxTypeTest, Utility) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType,
                                 switches::kUtilityProcess);

  base::CommandLine command_line2(command_line);
  SetCommandLineFlagsForSandboxType(&command_line2, Sandbox::kNetwork);
  EXPECT_EQ(Sandbox::kNetwork, SandboxTypeFromCommandLine(command_line2));

  base::CommandLine command_line3(command_line);
  SetCommandLineFlagsForSandboxType(&command_line3, Sandbox::kCdm);
  EXPECT_EQ(Sandbox::kCdm, SandboxTypeFromCommandLine(command_line3));

  base::CommandLine command_line4(command_line);
  SetCommandLineFlagsForSandboxType(&command_line4, Sandbox::kNoSandbox);
  EXPECT_EQ(Sandbox::kNoSandbox, SandboxTypeFromCommandLine(command_line4));

#if BUILDFLAG(ENABLE_PPAPI) && !BUILDFLAG(IS_WIN)
  base::CommandLine command_line5(command_line);
  SetCommandLineFlagsForSandboxType(&command_line5, Sandbox::kPpapi);
  EXPECT_EQ(Sandbox::kPpapi, SandboxTypeFromCommandLine(command_line5));
#endif

  base::CommandLine command_line6(command_line);
  SetCommandLineFlagsForSandboxType(&command_line6, Sandbox::kService);
  EXPECT_EQ(Sandbox::kService, SandboxTypeFromCommandLine(command_line6));

  base::CommandLine command_line7(command_line);
  SetCommandLineFlagsForSandboxType(&command_line7, Sandbox::kPrintCompositor);
  EXPECT_EQ(Sandbox::kPrintCompositor,
            SandboxTypeFromCommandLine(command_line7));

  base::CommandLine command_line8(command_line);
  SetCommandLineFlagsForSandboxType(&command_line8, Sandbox::kAudio);
  EXPECT_EQ(Sandbox::kAudio, SandboxTypeFromCommandLine(command_line8));

  base::CommandLine command_line9(command_line);
  SetCommandLineFlagsForSandboxType(&command_line9,
                                    Sandbox::kSpeechRecognition);
  EXPECT_EQ(Sandbox::kSpeechRecognition,
            SandboxTypeFromCommandLine(command_line9));

#if BUILDFLAG(IS_WIN)
  base::CommandLine command_line10(command_line);
  SetCommandLineFlagsForSandboxType(&command_line10, Sandbox::kXrCompositing);
  EXPECT_EQ(Sandbox::kXrCompositing,
            SandboxTypeFromCommandLine(command_line10));

  base::CommandLine command_line11(command_line);
  SetCommandLineFlagsForSandboxType(&command_line11,
                                    Sandbox::kNoSandboxAndElevatedPrivileges);
  EXPECT_EQ(Sandbox::kNoSandboxAndElevatedPrivileges,
            SandboxTypeFromCommandLine(command_line11));

  base::CommandLine command_line12(command_line);
  SetCommandLineFlagsForSandboxType(&command_line12, Sandbox::kPdfConversion);
  EXPECT_EQ(Sandbox::kPdfConversion,
            SandboxTypeFromCommandLine(command_line12));
#endif

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  base::CommandLine command_line13(command_line);
  SetCommandLineFlagsForSandboxType(&command_line13, Sandbox::kPrintBackend);
  EXPECT_EQ(Sandbox::kPrintBackend, SandboxTypeFromCommandLine(command_line13));
#endif

  base::CommandLine command_line14(command_line);
  command_line14.AppendSwitchASCII(switches::kServiceSandboxType,
                                   switches::kNoneSandbox);
  EXPECT_EQ(Sandbox::kNoSandbox, SandboxTypeFromCommandLine(command_line14));

  base::CommandLine command_line15(command_line);
  SetCommandLineFlagsForSandboxType(&command_line15, Sandbox::kServiceWithJit);
  EXPECT_EQ(Sandbox::kServiceWithJit,
            SandboxTypeFromCommandLine(command_line15));

  command_line.AppendSwitch(switches::kNoSandbox);
  EXPECT_EQ(Sandbox::kNoSandbox, SandboxTypeFromCommandLine(command_line));
}

TEST(SandboxTypeTest, UtilityDeath) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType,
                                 switches::kUtilityProcess);

  EXPECT_DEATH_IF_SUPPORTED(SandboxTypeFromCommandLine(command_line), "");

  // kGPU not valid for utility processes.
  base::CommandLine command_line1(command_line);
  command_line1.AppendSwitchASCII(switches::kServiceSandboxType, "gpu");
  EXPECT_DEATH_IF_SUPPORTED(SandboxTypeFromCommandLine(command_line1), "");

  base::CommandLine command_line2(command_line);
  command_line2.AppendSwitchASCII(switches::kServiceSandboxType, "bogus");
  EXPECT_DEATH_IF_SUPPORTED(SandboxTypeFromCommandLine(command_line2), "");
}

TEST(SandboxTypeTest, GPU) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType, switches::kGpuProcess);
  SetCommandLineFlagsForSandboxType(&command_line, Sandbox::kGpu);
  EXPECT_EQ(Sandbox::kGpu, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitchASCII(switches::kServiceSandboxType, "network");
  EXPECT_EQ(Sandbox::kGpu, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitch(switches::kNoSandbox);
  EXPECT_EQ(Sandbox::kNoSandbox, SandboxTypeFromCommandLine(command_line));
}

#if BUILDFLAG(ENABLE_PPAPI) && !BUILDFLAG(IS_WIN)
TEST(SandboxTypeTest, PPAPIPlugin) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType,
                                 switches::kPpapiPluginProcess);
  SetCommandLineFlagsForSandboxType(&command_line, Sandbox::kPpapi);
  EXPECT_EQ(Sandbox::kPpapi, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitchASCII(switches::kServiceSandboxType, "network");
  EXPECT_EQ(Sandbox::kPpapi, SandboxTypeFromCommandLine(command_line));

  command_line.AppendSwitch(switches::kNoSandbox);
  EXPECT_EQ(Sandbox::kNoSandbox, SandboxTypeFromCommandLine(command_line));
}
#endif  // BUILDFLAG(ENABLE_PPAPI)

TEST(SandboxTypeTest, Nonesuch) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kProcessType, "nonesuch");
  // If tested here would CHECK.

  command_line.AppendSwitchASCII(switches::kServiceSandboxType, "network");
  // If tested here would CHECK.

  // With kNoSandbox will parse the command line correctly.
  command_line.AppendSwitch(switches::kNoSandbox);
  EXPECT_EQ(Sandbox::kNoSandbox, SandboxTypeFromCommandLine(command_line));
}

TEST(SandboxTypeTest, ElevatedPrivileges) {
  // Tests that the "no sandbox and elevated privileges" which is Windows
  // specific default to no sandbox on non Windows platforms.
  Sandbox elevated_type =
      UtilitySandboxTypeFromString(switches::kNoneSandboxAndElevatedPrivileges);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(Sandbox::kNoSandboxAndElevatedPrivileges, elevated_type);
#else
  EXPECT_EQ(Sandbox::kNoSandbox, elevated_type);
#endif
}

}  // namespace policy
}  // namespace sandbox
