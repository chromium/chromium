// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <memory>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

class AddressSanitizerTests : public ::testing::Test {
 public:
  void SetUp() override {
    env_ = base::Environment::Create();
    had_asan_options_ = env_->GetVar("ASAN_OPTIONS", &old_asan_options_);
  }

  void TearDown() override {
    if (had_asan_options_)
      ASSERT_TRUE(env_->SetVar("ASAN_OPTIONS", old_asan_options_));
    else
      env_->UnSetVar("ASAN_OPTIONS");
  }

 protected:
  std::unique_ptr<base::Environment> env_;
  bool had_asan_options_;
  std::string old_asan_options_;
};

SBOX_TESTS_COMMAND int AddressSanitizerTests_Report(int argc, wchar_t** argv) {
  // AddressSanitizer should detect an out of bounds write (heap buffer
  // overflow) in this code.
  volatile int idx = 42;
  int* volatile blah = new int[42];
  blah[idx] = 42;
  delete[] blah;
  return SBOX_TEST_FAILED;
}

TEST_F(AddressSanitizerTests, TestAddressSanitizer) {
// This test is only supposed to work when using AddressSanitizer.
// However, ASan/Win is not on the CQ yet, so compiler breakages may get into
// the code unnoticed.  To avoid that, we compile this test in all Windows
// builds, but only run the AddressSanitizer-specific part of the test when
// compiled with AddressSanitizer.
#if defined(ADDRESS_SANITIZER)
  bool asan_build = true;
#else
  bool asan_build = false;
#endif
  base::ScopedTempDir temp_directory;
  base::FilePath temp_file_name;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  ASSERT_TRUE(
      CreateTemporaryFileInDir(temp_directory.GetPath(), &temp_file_name));

  SECURITY_ATTRIBUTES attrs = {};
  attrs.nLength = sizeof(attrs);
  attrs.bInheritHandle = true;
  base::win::ScopedHandle tmp_handle(
      CreateFile(temp_file_name.value().c_str(), GENERIC_WRITE,
                 FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, &attrs,
                 OPEN_EXISTING, 0, nullptr));
  EXPECT_TRUE(tmp_handle.IsValid());

  TestRunner runner;
  ASSERT_EQ(SBOX_ALL_OK, runner.GetPolicy()->SetStderrHandle(tmp_handle.Get()));

  base::FilePath exe;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &exe));
  base::FilePath pdb_path = exe.DirName().Append(L"*.pdb");
  ASSERT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_READONLY,
                               pdb_path.value().c_str()));

  env_->SetVar("ASAN_OPTIONS", "exitcode=123");
  if (asan_build) {
    int result = runner.RunTest(L"AddressSanitizerTests_Report");
    EXPECT_EQ(123, result);

    std::string data;
    ASSERT_TRUE(base::ReadFileToString(base::FilePath(temp_file_name), &data));
    // Redirection uses a feature that was added in Windows Vista.
    ASSERT_TRUE(
        strstr(data.c_str(), "ERROR: AddressSanitizer: heap-buffer-overflow"))
        << "There doesn't seem to be an ASan report:\n"
        << data;
    ASSERT_TRUE(strstr(data.c_str(), "AddressSanitizerTests_Report"))
        << "The ASan report doesn't appear to be symbolized:\n"
        << data;
    std::string source_file_basename(__FILE__);
    size_t last_slash = source_file_basename.find_last_of("/\\");
    last_slash = last_slash == std::string::npos ? 0 : last_slash + 1;
    ASSERT_TRUE(strstr(data.c_str(), &source_file_basename[last_slash]))
        << "The stack trace doesn't have a correct filename:\n"
        << data;
  } else {
    LOG(WARNING) << "Not an AddressSanitizer build, skipping the run.";
  }
}

}  // namespace sandbox
