// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/seatbelt_extension.h"

#include <unistd.h>

#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "sandbox/mac/sandbox_compiler.h"
#include "sandbox/mac/seatbelt_extension_token.h"
#include "testing/multiprocess_func_list.h"

namespace sandbox {
namespace {

const char kSandboxProfile[] =
    "(version 1)\n"
    "(deny default (with no-log))\n"
    "(allow file-read* (extension \"com.apple.app-sandbox.read\"))";

const char kTestData[] = "hello world";
constexpr int kTestDataLen = base::size(kTestData);

const char kSwitchFile[] = "test-file";
const char kSwitchExtension[] = "test-extension";

class SeatbeltExtensionTest : public base::MultiProcessTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.GetPath().AppendASCII("sbox.test");

    ASSERT_EQ(kTestDataLen,
              base::WriteFile(file_path_, kTestData, kTestDataLen));
  }

  base::FilePath file_path() { return file_path_; }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath file_path_;
};

TEST_F(SeatbeltExtensionTest, FileReadAccess) {
  base::CommandLine command_line(
      base::GetMultiProcessTestChildBaseCommandLine());

  auto token = sandbox::SeatbeltExtension::Issue(
      sandbox::SeatbeltExtension::FILE_READ, file_path().value());
  ASSERT_TRUE(token.get());

  // Ensure any symlinks in the path are canonicalized.
  base::FilePath path = base::MakeAbsoluteFilePath(file_path());
  ASSERT_FALSE(path.empty());

  command_line.AppendSwitchPath(kSwitchFile, path);
  command_line.AppendSwitchASCII(kSwitchExtension, token->token());

  base::Process test_child = base::SpawnMultiProcessTestChild(
      "FileReadAccess", command_line, base::LaunchOptions());

  int exit_code = 42;
  test_child.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                    &exit_code);
  EXPECT_EQ(0, exit_code);
}

MULTIPROCESS_TEST_MAIN(FileReadAccess) {
  sandbox::SandboxCompiler compiler(kSandboxProfile);
  std::string error;
  CHECK(compiler.CompileAndApplyProfile(&error)) << error;

  auto* command_line = base::CommandLine::ForCurrentProcess();

  base::FilePath file_path = command_line->GetSwitchValuePath(kSwitchFile);
  CHECK(!file_path.empty());
  const char* path = file_path.value().c_str();

  std::string token_str = command_line->GetSwitchValueASCII(kSwitchExtension);
  CHECK(!token_str.empty());

  auto token = sandbox::SeatbeltExtensionToken::CreateForTesting(token_str);
  auto extension = sandbox::SeatbeltExtension::FromToken(std::move(token));
  CHECK(extension);
  CHECK(token.token().empty());

  // Without consuming the extension, file access is denied.
  errno = 0;
  base::ScopedFD fd(open(path, O_RDONLY));
  CHECK_EQ(-1, fd.get());
  CHECK_EQ(EPERM, errno);

  CHECK(extension->Consume());

  // After consuming the extension, file access is still denied for writing.
  errno = 0;
  fd.reset(open(path, O_RDWR));
  CHECK_EQ(-1, fd.get());
  CHECK_EQ(EPERM, errno);

  // ... but it is allowed to read.
  errno = 0;
  fd.reset(open(path, O_RDONLY));
  PCHECK(fd.get() > 0);

  // Close the file and revoke the extension.
  fd.reset();
  extension->Revoke();

  // File access is denied again.
  errno = 0;
  fd.reset(open(path, O_RDONLY));
  CHECK_EQ(-1, fd.get());
  CHECK_EQ(EPERM, errno);

  // Re-acquire the access by using the token, but this time consume it
  // permanetly.
  token = sandbox::SeatbeltExtensionToken::CreateForTesting(token_str);
  extension = sandbox::SeatbeltExtension::FromToken(std::move(token));
  CHECK(extension);
  CHECK(extension->ConsumePermanently());

  // Check that reading still works.
  errno = 0;
  fd.reset(open(path, O_RDONLY));
  PCHECK(fd.get() > 0);

  return 0;
}

}  // namespace
}  // namespace sandbox
