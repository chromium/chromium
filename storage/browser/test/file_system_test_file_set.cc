// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "storage/browser/test/file_system_test_file_set.h"

#include <stdint.h>

#include <limits>
#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

const FileSystemTestCaseRecord kRegularFileSystemTestCases[] = {
    {true, FILE_PATH_LITERAL("dir a"), 0, TestBlockAction::CHILD_BLOCKED},
    {true, FILE_PATH_LITERAL("dir a/dir A"), 0, TestBlockAction::ALLOWED},
    {true, FILE_PATH_LITERAL("dir a/dir d"), 0, TestBlockAction::CHILD_BLOCKED},
    {true, FILE_PATH_LITERAL("dir a/dir d/dir e"), 0,
     TestBlockAction::CHILD_BLOCKED},
    {true, FILE_PATH_LITERAL("dir a/dir d/dir e/dir f"), 0,
     TestBlockAction::ALLOWED},
    {true, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g"), 0,
     TestBlockAction::CHILD_BLOCKED},
    {true, FILE_PATH_LITERAL("dir a/dir d/dir e/dir h"), 0,
     TestBlockAction::BLOCKED},
    {true, FILE_PATH_LITERAL("dir b"), 0, TestBlockAction::BLOCKED},
    {true, FILE_PATH_LITERAL("dir b/dir a"), 0,
     TestBlockAction::PARENT_BLOCKED},
    {true, FILE_PATH_LITERAL("dir c"), 0, TestBlockAction::ALLOWED},
    {false, FILE_PATH_LITERAL("file 0"), 38, TestBlockAction::ALLOWED},
    {false, FILE_PATH_LITERAL("file 2"), 60, TestBlockAction::BLOCKED},
    {false, FILE_PATH_LITERAL("file 3"), 0, TestBlockAction::ALLOWED},
    {false, FILE_PATH_LITERAL("dir a/file 0"), 39, TestBlockAction::ALLOWED},
    {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 0"), 40,
     TestBlockAction::ALLOWED},
    {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 1"), 41,
     TestBlockAction::ALLOWED},
    {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 2"), 42,
     TestBlockAction::BLOCKED},
    {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 3"), 43,
     TestBlockAction::ALLOWED},
    {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir h/file 0"), 44,
     TestBlockAction::PARENT_BLOCKED},
    {false, FILE_PATH_LITERAL("dir a/dir d/dir e/dir h/file 1"), 45,
     TestBlockAction::PARENT_BLOCKED},
    {false, FILE_PATH_LITERAL("dir b/file 0"), 46,
     TestBlockAction::PARENT_BLOCKED},
    {false, FILE_PATH_LITERAL("dir b/file 1"), 47,
     TestBlockAction::PARENT_BLOCKED},
};

const size_t kRegularFileSystemTestCaseSize =
    std::size(kRegularFileSystemTestCases);

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
  for (size_t i = 0; i < std::size(kRegularFileSystemTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "Creating kRegularTestCases " << i);
    SetUpOneFileSystemTestCase(root_path, kRegularFileSystemTestCases[i]);
  }
}

}  // namespace storage
