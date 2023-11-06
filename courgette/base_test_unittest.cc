// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/base_test_unittest.h"

#include "base/files/file_util.h"
#include "base/path_service.h"

void BaseTest::SetUp() {
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_dir_));
  test_dir_ = test_dir_.AppendASCII("courgette");
  test_dir_ = test_dir_.AppendASCII("testdata");
}

void BaseTest::TearDown() {
}

//  Reads a test file into a string.
std::string BaseTest::FileContents(const char* file_name) const {
  base::FilePath file_path = test_dir_;
  file_path = file_path.AppendASCII(file_name);
  std::string file_bytes;

  EXPECT_TRUE(base::ReadFileToString(file_path, &file_bytes));

  return file_bytes;
}

std::string BaseTest::FilesContents(std::list<std::string> file_names) const {
  std::string result;

  std::list<std::string>::iterator file_name = file_names.begin();

  while (file_name != file_names.end()) {
    result += FileContents(file_name->c_str());
    file_name++;
  }

  return result;
}
