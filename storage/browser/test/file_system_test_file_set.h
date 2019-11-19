// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_FILE_SYSTEM_TEST_FILE_SET_H_
#define STORAGE_BROWSER_TEST_FILE_SYSTEM_TEST_FILE_SET_H_

#include <stddef.h>
#include <stdint.h>

#include <set>

#include "base/files/file_path.h"

// Common test data structures and test cases.

namespace content {

struct FileSystemTestCaseRecord {
  bool is_directory;
  const base::FilePath::CharType path[64];
  int64_t data_file_size;
};

extern const FileSystemTestCaseRecord kRegularFileSystemTestCases[];
extern const size_t kRegularFileSystemTestCaseSize;

size_t GetRegularFileSystemTestCaseSize();

// Creates one file or directory specified by |record|.
void SetUpOneFileSystemTestCase(const base::FilePath& root_path,
                                const FileSystemTestCaseRecord& record);

// Creates the files and directories specified in kRegularTestCases.
void SetUpRegularFileSystemTestCases(const base::FilePath& root_path);

}  // namespace content

#endif  // STORAGE_BROWSER_TEST_FILE_SYSTEM_TEST_FILE_SET_H_
