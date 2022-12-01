// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/seatbelt.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/mac_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "sandbox/mac/seatbelt.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace sandbox {

using SeatbeltTest = base::MultiProcessTest;

MULTIPROCESS_TEST_MAIN(SandboxCheckTestProcess) {
  CHECK(!Seatbelt::IsSandboxed());
  const char* profile =
      "(version 1)"
      "(deny default (with no-log))";

  std::string error;
  CHECK(Seatbelt::Init(profile, 0, &error));
  CHECK(Seatbelt::IsSandboxed());

  return 0;
}

TEST_F(SeatbeltTest, SandboxCheckTest) {
  base::Process process = SpawnChild("SandboxCheckTestProcess");
  ASSERT_TRUE(process.IsValid());
  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  EXPECT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(Ftruncate) {
  const char* profile =
      "(version 1)"
      "(deny default (with no-log))";
  std::string error;
  CHECK(Seatbelt::Init(profile, 0, &error)) << error;

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
TEST_F(SeatbeltTest, Ftruncate) {
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
