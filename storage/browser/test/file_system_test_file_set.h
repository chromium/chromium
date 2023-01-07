// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_FILE_SYSTEM_TEST_FILE_SET_H_
#define STORAGE_BROWSER_TEST_FILE_SYSTEM_TEST_FILE_SET_H_

#include <stddef.h>
#include <stdint.h>

#include "base/files/file_path.h"

// Common test data structures and test cases.

namespace storage {

// For tests using blocking, this indicates whether a file should be blocked or
// allowed and additionally marks the expectation.
enum class TestBlockAction {
  // Allow the transfer of the file/directory. Additionally, this indicates that
  // all its children and parents are allowed.
  ALLOWED,
  // Block the file/directory.
  BLOCKED,
  // A parent/ancestor of this file/directory is blocked. This file/directory
  // should never be reached on a recursive operation (if blocking is enabled).
  PARENT_BLOCKED,
  // Allow the transfer of the directory. Additionally, this indicates that some
  // child/descendant of this directory is blocked.
  CHILD_BLOCKED,
};

struct FileSystemTestCaseRecord {
  bool is_directory;
  const base::FilePath::CharType path[64];
  int64_t data_file_size;
  TestBlockAction block_action;
};

extern const FileSystemTestCaseRecord kRegularFileSystemTestCases[];
extern const size_t kRegularFileSystemTestCaseSize;

size_t GetRegularFileSystemTestCaseSize();

// Creates one file or directory specified by |record|.
void SetUpOneFileSystemTestCase(const base::FilePath& root_path,
                                const FileSystemTestCaseRecord& record);

// Creates the files and directories specified in kRegularTestCases.
void SetUpRegularFileSystemTestCases(const base::FilePath& root_path);

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_FILE_SYSTEM_TEST_FILE_SET_H_
