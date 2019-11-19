// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/file_system_test_file_set.h"

#include <stdint.h>

#include <limits>
#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

const FileSystemTestCaseRecord kRegularFileSystemTestCases[] = {
    {true, FILE_PATH_LITERAL("dir a"), 0},
    {true, FILE_PATH_LITERAL("dir a/dir A"), 0},
    {true, FILE_PATH_LITERAL("dir a/dir d"), 0},
    {true, FILE_PATH_LITERAL("dir a/dir d/dir e"), 0},
    {true, FILE_PATH_LITERAL("dir a/dir d/dir e/dir f"), 0},
    {true, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g"), 0},
    {true, FILE_PATH_LITERAL("dir a/dir d/dir e/dir h"), 0},
    {true, FILE_PATH_LITERAL("dir b"), 0},
    {true, FILE_PATH_LITERAL("dir b/dir a"), 0},
    {true, FILE_PATH_LITERAL("dir c"), 0},
    {false, FILE_PATH_LITERAL("file 0"), 38},
    {false, FILE_PATH_LITERAL("file 2"), 60},
    {false, FILE_PATH_LITERAL("file 3"), 0},
    {false, FILE_PATH_LITERAL("dir a/file 0"), 39},
    {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 0"), 40},
    {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 1"), 41},
    {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 2"), 42},
    {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 3"), 50},
};

const size_t kRegularFileSystemTestCaseSize =
    base::size(kRegularFileSystemTestCases);

void SetUpOneFileSystemTestCase(const base::FilePath& root_path,
                                const FileSystemTestCaseRecord& test_case) {
  base::FilePath path = root_path.Append(test_case.path);
  if (test_case.is_directory) {
    ASSERT_TRUE(base::CreateDirectory(path));
    return;
  }
  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(file.IsValid());
  if (test_case.data_file_size) {
    std::string content = base::RandBytesAsString(test_case.data_file_size);
    EXPECT_LE(test_case.data_file_size, std::numeric_limits<int32_t>::max());
    ASSERT_EQ(static_cast<int>(content.size()),
              file.Write(0, content.data(), static_cast<int>(content.size())));
  }
}

void SetUpRegularFileSystemTestCases(const base::FilePath& root_path) {
  for (size_t i = 0; i < base::size(kRegularFileSystemTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Creating kRegularTestCases " << i);
    SetUpOneFileSystemTestCase(root_path, kRegularFileSystemTestCases[i]);
  }
}

}  // namespace content
