// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

SBOX_TESTS_COMMAND int HandleInheritanceTests_PrintToStdout(int argc,
                                                            wchar_t** argv) {
  printf("Example output to stdout\n");
  return SBOX_TEST_SUCCEEDED;
}

TEST(HandleInheritanceTests, TestStdoutInheritance) {
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
  ASSERT_TRUE(tmp_handle.is_valid());

  TestRunner runner;
  ASSERT_EQ(SBOX_ALL_OK, runner.GetPolicy()->SetStdoutHandle(tmp_handle.get()));
  int result = runner.RunTest(L"HandleInheritanceTests_PrintToStdout");
  ASSERT_EQ(SBOX_TEST_SUCCEEDED, result);

  std::string data;
  ASSERT_TRUE(base::ReadFileToString(base::FilePath(temp_file_name), &data));
  // Redirection uses a feature that was added in Windows Vista.
  ASSERT_EQ("Example output to stdout\r\n", data);
}

}  // namespace sandbox
