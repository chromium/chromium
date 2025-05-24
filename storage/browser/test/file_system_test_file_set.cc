// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/file_system_test_file_set.h"

#include <stdint.h>

#include <limits>
#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/rand_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

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
    const std::vector<uint8_t> content =
        base::RandBytesAsVector(test_case.data_file_size);
    EXPECT_LE(test_case.data_file_size, std::numeric_limits<int32_t>::max());
    ASSERT_THAT(file.Write(0, content), testing::Optional(content.size()));
  }
}

void SetUpRegularFileSystemTestCases(const base::FilePath& root_path) {
  size_t count = 0;
  for (const auto& test_case : kRegularFileSystemTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Creating kRegularTestCases " << count++);
    SetUpOneFileSystemTestCase(root_path, test_case);
  }
}

}  // namespace storage
