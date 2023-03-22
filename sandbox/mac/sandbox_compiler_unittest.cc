// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <unistd.h>

#include "base/process/kill.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "sandbox/mac/sandbox_compiler.h"
#include "sandbox/mac/sandbox_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace sandbox {

namespace {

constexpr char kTestParamSwitch[] = "sandbox-compiler-target";

SandboxCompiler::Target GetParamInChild() {
  std::string target_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kTestParamSwitch);
  CHECK(!target_str.empty());
  int target_int;
  CHECK(base::StringToInt(target_str, &target_int));
  return static_cast<SandboxCompiler::Target>(target_int);
}

}  // namespace

class SandboxCompilerTest
    : public SandboxTest,
      public testing::WithParamInterface<SandboxCompiler::Target> {
 protected:
  base::CommandLine MakeCmdLine(const std::string& procname) override {
    base::CommandLine command_line =
        base::MultiProcessTest::MakeCmdLine(procname);
    command_line.AppendSwitchASCII(
        kTestParamSwitch, base::NumberToString(static_cast<int>(GetParam())));
    return command_line;
  }
};

MULTIPROCESS_TEST_MAIN(BasicProfileProcess) {
  SandboxCompiler compiler(GetParamInChild());
  compiler.SetProfile(R"(
      (version 1)
      (deny default (with no-log))
      (allow file-read* file-write* (literal "/"))
  )");

  std::string error;
  CHECK(compiler.CompileAndApplyProfile(error));

  return 0;
}

TEST_P(SandboxCompilerTest, BasicProfileTest) {
  base::Process process = SpawnChild("BasicProfileProcess");
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(BasicProfileWithParamProcess) {
  SandboxCompiler compiler(GetParamInChild());
  compiler.SetProfile(R"(
      (version 1)
      (deny default (with no-log))
      (allow file-read* file-write* (literal (param "DIR")))
  )");
  CHECK(compiler.SetParameter("DIR", "/"));

  std::string error;
  CHECK(compiler.CompileAndApplyProfile(error)) << error;

  return 0;
}

TEST_P(SandboxCompilerTest, BasicProfileTestWithParam) {
  base::Process process = SpawnChild("BasicProfileWithParamProcess");
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(ProfileFunctionalProcess) {
  SandboxCompiler compiler(GetParamInChild());
  compiler.SetProfile(R"(
      (version 1)
      (deny default (with no-log))
      (allow file-read-data file-read-metadata (literal "/dev/urandom"))
  )");

  std::string error;
  CHECK(compiler.CompileAndApplyProfile(error)) << error;

  // The profile compiled and applied successfully, now try and read 1 byte from
  // /dev/urandom.
  uint8_t byte;
  int fd = open("/dev/urandom", O_RDONLY);
  CHECK_NE(fd, -1);

  EXPECT_TRUE(read(fd, &byte, sizeof(byte)) == sizeof(byte));

  return 0;
}

TEST_P(SandboxCompilerTest, ProfileFunctionalityTest) {
  base::Process process = SpawnChild("ProfileFunctionalProcess");
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(ProfileFunctionalTestWithParamsProcess) {
  SandboxCompiler compiler(GetParamInChild());
  compiler.SetProfile(R"(
      (version 1)
      (deny default (with no-log))
      (if (string=? (param "ALLOW_FILE") "TRUE")
          (allow file-read-data file-read-metadata (literal (param "URANDOM")))
      )
  )");

  CHECK(compiler.SetBooleanParameter("ALLOW_FILE", true));
  CHECK(!compiler.SetParameter("ALLOW_FILE", "duplicate key is not allowed"));
  CHECK(compiler.SetParameter("URANDOM", "/dev/urandom"));

  std::string error;
  CHECK(compiler.CompileAndApplyProfile(error)) << error;

  // The profile compiled and applied successfully, now try and read 1 byte from
  // /dev/urandom.
  uint8_t byte;
  int fd = open("/dev/urandom", O_RDONLY);
  CHECK_NE(fd, -1);

  EXPECT_TRUE(read(fd, &byte, sizeof(byte)) == sizeof(byte));

  // Make sure the sandbox isn't overly permissive.
  struct stat st;
  EXPECT_EQ(stat("/", &st), -1);

  return 0;
}

TEST_P(SandboxCompilerTest, ProfileFunctionalityTestWithParams) {
  base::Process process = SpawnChild("ProfileFunctionalTestWithParamsProcess");
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(ProfileFunctionalityTestErrorProcess) {
  SandboxCompiler compiler(GetParamInChild());
  compiler.SetProfile("(+ 5 a)");

  // Make sure that this invalid profile results in an error returned.
  std::string error;
  CHECK_EQ(error, "");
  CHECK(!compiler.CompileAndApplyProfile(error)) << error;
  CHECK_NE(error, "");

  return 0;
}

TEST_P(SandboxCompilerTest, ProfileFunctionalityTestError) {
  base::Process process = SpawnChild("ProfileFunctionalityTestErrorProcess");
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, 0);
}

TEST_P(SandboxCompilerTest, DuplicateKeys) {
  SandboxCompiler compiler(GetParam());
  compiler.SetProfile("(version 1)(deny default)");

  EXPECT_TRUE(compiler.SetBooleanParameter("key1", true));
  EXPECT_FALSE(compiler.SetBooleanParameter("key1", false));
  EXPECT_TRUE(compiler.SetBooleanParameter("key2", false));
  EXPECT_TRUE(compiler.SetParameter("key3", "value"));
  EXPECT_FALSE(compiler.SetParameter("key3", "value"));

  mac::SandboxPolicy policy;
  std::string error;
  ASSERT_TRUE(compiler.CompilePolicyToProto(policy, error)) << error;
  if (GetParam() == SandboxCompiler::Target::kSource) {
    EXPECT_EQ(3, policy.source().params_size());
    EXPECT_FALSE(policy.source().profile().empty());
    EXPECT_TRUE(policy.compiled().data().empty());
  } else {
    EXPECT_EQ(0, policy.source().params_size());
    EXPECT_TRUE(policy.source().profile().empty());
    EXPECT_FALSE(policy.compiled().data().empty());
  }
}

INSTANTIATE_TEST_SUITE_P(Target,
                         SandboxCompilerTest,
                         testing::Values(SandboxCompiler::Target::kSource,
                                         SandboxCompiler::Target::kCompiled));

}  // namespace sandbox
