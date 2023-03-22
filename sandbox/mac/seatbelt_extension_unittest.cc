// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/seatbelt_extension.h"

#include <sys/stat.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "sandbox/mac/sandbox_compiler.h"
#include "sandbox/mac/sandbox_test.h"
#include "sandbox/mac/seatbelt_extension_token.h"
#include "testing/multiprocess_func_list.h"

namespace sandbox {
namespace {

const char kSandboxProfile[] = R"(
  (version 1)
  (deny default (with no-log))
  (allow file-read* (extension "com.apple.app-sandbox.read"))
  (allow file-read* file-write*
    (extension "com.apple.app-sandbox.read-write")
  )
)";

const char kTestData[] = "hello world";
const char kSwitchFile[] = "test-file";
const char kSwitchExtension[] = "test-extension";

class SeatbeltExtensionTest : public SandboxTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.GetPath().AppendASCII("sbox.test");

    ASSERT_TRUE(base::WriteFile(file_path_, kTestData));
  }

  base::FilePath file_path() { return file_path_; }

  base::Process SpawnChildForPathWithToken(
      const std::string& procname,
      const base::FilePath& path,
      const SeatbeltExtensionToken& token) {
    // Ensure any symlinks in the path are canonicalized.
    const base::FilePath canonicalized_path = base::MakeAbsoluteFilePath(path);
    const std::string token_value = token.token();
    CHECK(!canonicalized_path.empty());
    CHECK(!token_value.empty());
    return SpawnChild(
        procname,
        base::BindLambdaForTesting([&](base::CommandLine& command_line) {
          command_line.AppendSwitchPath(kSwitchFile, canonicalized_path);
          command_line.AppendSwitchASCII(kSwitchExtension, token_value);
        }));
  }

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

  base::Process test_child =
      SpawnChildForPathWithToken("FileReadAccess", file_path(), *token);
  int exit_code = 42;
  test_child.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                    &exit_code);
  EXPECT_EQ(0, exit_code);
}

MULTIPROCESS_TEST_MAIN(FileReadAccess) {
  sandbox::SandboxCompiler compiler;
  compiler.SetProfile(kSandboxProfile);
  std::string error;
  CHECK(compiler.CompileAndApplyProfile(error)) << error;

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

TEST_F(SeatbeltExtensionTest, DirReadWriteAccess) {
  base::CommandLine command_line(
      base::GetMultiProcessTestChildBaseCommandLine());

  auto token = sandbox::SeatbeltExtension::Issue(
      sandbox::SeatbeltExtension::FILE_READ_WRITE,
      file_path().DirName().value());
  ASSERT_TRUE(token.get());

  base::Process test_child =
      SpawnChildForPathWithToken("DirReadWriteAccess", file_path(), *token);
  int exit_code = 42;
  test_child.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                    &exit_code);
  EXPECT_EQ(0, exit_code);
}

MULTIPROCESS_TEST_MAIN(DirReadWriteAccess) {
  sandbox::SandboxCompiler compiler;
  compiler.SetProfile(kSandboxProfile);
  std::string error;
  CHECK(compiler.CompileAndApplyProfile(error)) << error;

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

  CHECK(extension->ConsumePermanently());

  // After consuming the extension, file read/write access is allowed.
  errno = 0;
  fd.reset(open(path, O_RDWR));
  PCHECK(fd.get() > 0);

  char c = 'a';
  PCHECK(write(fd.get(), &c, sizeof(c)) == sizeof(c));

  // A new file can be created in the extension directory.
  base::FilePath new_file = file_path.DirName().AppendASCII("new_file.txt");
  fd.reset(open(path, O_RDWR | O_CREAT));
  PCHECK(fd.get() > 0);

  // Test reading and writing to the new file.
  PCHECK(write(fd.get(), &c, sizeof(c)) == sizeof(c));

  PCHECK(lseek(fd.get(), 0, SEEK_SET) == 0);

  c = 'b';
  PCHECK(read(fd.get(), &c, sizeof(c)) == sizeof(c));
  CHECK_EQ(c, 'a');

  // Accessing the parent directory is forbidden.
  errno = 0;
  struct stat sb;
  PCHECK(stat(file_path.DirName().DirName().value().c_str(), &sb) == -1);
  CHECK_EQ(EPERM, errno);

  // ... but the dir itself is okay.
  errno = 0;
  PCHECK(stat(file_path.DirName().value().c_str(), &sb) == 0);

  return 0;
}

}  // namespace
}  // namespace sandbox
