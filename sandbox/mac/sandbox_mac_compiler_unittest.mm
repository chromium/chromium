// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/mac_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "sandbox/mac/sandbox_compiler.h"
#include "sandbox/mac/seatbelt.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace sandbox {

class SandboxMacCompilerTest : public base::MultiProcessTest {};

MULTIPROCESS_TEST_MAIN(BasicProfileProcess) {
  std::string profile =
      "(version 1)"
      "(deny default (with no-log))"
      "(allow file-read* file-write* (literal \"/\"))";

  SandboxCompiler compiler(profile);

  std::string error;
  CHECK(compiler.CompileAndApplyProfile(&error));

  return 0;
}

TEST_F(SandboxMacCompilerTest, BasicProfileTest) {
  base::Process process = SpawnChild("BasicProfileProcess");
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(BasicProfileWithParamProcess) {
  std::string profile =
      "(version 1)"
      "(deny default (with no-log))"
      "(allow file-read* file-write* (literal (param \"DIR\")))";

  SandboxCompiler compiler(profile);
  CHECK(compiler.InsertStringParam("DIR", "/"));

  std::string error;
  CHECK(compiler.CompileAndApplyProfile(&error));

  return 0;
}

TEST_F(SandboxMacCompilerTest, BasicProfileTestWithParam) {
  base::Process process = SpawnChild("BasicProfileWithParamProcess");
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(ProfileFunctionalProcess) {
  std::string profile =
      "(version 1)"
      "(deny default (with no-log))"
      "(allow file-read-data file-read-metadata (literal \"/dev/urandom\"))";

  SandboxCompiler compiler(profile);

  std::string error;
  CHECK(compiler.CompileAndApplyProfile(&error));

  // The profile compiled and applied successfully, now try and read 1 byte from
  // /dev/urandom.
  uint8_t byte;
  int fd = open("/dev/urandom", O_RDONLY);
  CHECK_NE(fd, -1);

  EXPECT_TRUE(read(fd, &byte, sizeof(byte)) == sizeof(byte));

  return 0;
}

TEST_F(SandboxMacCompilerTest, ProfileFunctionalityTest) {
  base::Process process = SpawnChild("ProfileFunctionalProcess");
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(ProfileFunctionalTestWithParamsProcess) {
  std::string profile =
      "(version 1)"
      "(deny default (with no-log))"
      "(if (string=? (param \"ALLOW_FILE\") \"TRUE\")"
      "    (allow file-read-data file-read-metadata (literal (param "
      "\"URANDOM\"))))";

  SandboxCompiler compiler(profile);

  CHECK(compiler.InsertBooleanParam("ALLOW_FILE", true));
  CHECK(compiler.InsertStringParam("URANDOM", "/dev/urandom"));

  std::string error;
  CHECK(compiler.CompileAndApplyProfile(&error));

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

TEST_F(SandboxMacCompilerTest, ProfileFunctionalityTestWithParams) {
  base::Process process = SpawnChild("ProfileFunctionalTestWithParamsProcess");
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(ProfileFunctionalityTestErrorProcess) {
  std::string profile = "(+ 5 a)";

  SandboxCompiler compiler(profile);

  // Make sure that this invalid profile results in an error returned.
  std::string error;
  CHECK_EQ(error, "");
  CHECK(!compiler.CompileAndApplyProfile(&error));
  CHECK_NE(error, "");

  return 0;
}

TEST_F(SandboxMacCompilerTest, ProfileFunctionalityTestError) {
  base::Process process = SpawnChild("ProfileFunctionalityTestErrorProcess");
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(SandboxCheckTestProcess) {
  CHECK(!Seatbelt::IsSandboxed());
  std::string profile =
      "(version 1)"
      "(deny default (with no-log))";

  SandboxCompiler compiler(profile);
  std::string error;
  CHECK(compiler.CompileAndApplyProfile(&error));
  CHECK(Seatbelt::IsSandboxed());

  return 0;
}

TEST_F(SandboxMacCompilerTest, SandboxCheckTest) {
  base::Process process = SpawnChild("SandboxCheckTestProcess");
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(Ftruncate) {
  std::string profile = "(version 1)"
                        "(deny default (with no-log))";
  SandboxCompiler compiler(profile);
  std::string error;
  CHECK(compiler.CompileAndApplyProfile(&error)) << error;

  std::unique_ptr<base::Environment> env = base::Environment::Create();

  std::string fd_string;
  CHECK(env->GetVar("FD_TO_TRUNCATE", &fd_string));

  int fd;
  CHECK(base::StringToInt(fd_string, &fd));

  const char kTestBuf[] = "hello";
  CHECK_EQ(static_cast<ssize_t>(strlen(kTestBuf)),
           HANDLE_EINTR(write(fd, kTestBuf, strlen(kTestBuf))));

  return ftruncate(fd, 0) == 0 ? 0 : 15;
}

// Tests ftruncate() behavior on an inherited, open, writable FD. Prior to
// macOS 10.15, the sandbox did not permit ftruncate (but it did permit regular
// writing) on such FDs. This verifies the behavior before, on, and after macOS
// 10.15. See https://crbug.com/1084565 for details.
TEST_F(SandboxMacCompilerTest, Ftruncate) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::File file(
      temp_dir.GetPath().Append("file.txt"),
      base::File::FLAG_CREATE | base::File::FLAG_READ | base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());

  const std::string contents =
      "Wouldn't it be nice to be able to use ftruncate?\n";
  EXPECT_EQ(static_cast<int>(contents.length()),
            file.WriteAtCurrentPos(contents.data(), contents.length()));
  EXPECT_EQ(static_cast<int64_t>(contents.length()), file.GetLength());

  base::PlatformFile fd = file.GetPlatformFile();

  base::LaunchOptions options;
  options.fds_to_remap.emplace_back(fd, fd);
  options.environment["FD_TO_TRUNCATE"] = base::NumberToString(fd);

  base::Process process = SpawnChildWithOptions("Ftruncate", options);
  ASSERT_TRUE(process.IsValid());

  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));

  if (base::mac::IsAtLeastOS10_15()) {
    EXPECT_EQ(0, exit_code);
    EXPECT_EQ(0, file.GetLength());
  } else {
    EXPECT_EQ(15, exit_code);
    EXPECT_GT(file.GetLength(), static_cast<int64_t>(contents.length()));
  }
}

}  // namespace sandbox
