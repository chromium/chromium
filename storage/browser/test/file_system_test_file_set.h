// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_FILE_SYSTEM_TEST_FILE_SET_H_
#define STORAGE_BROWSER_TEST_FILE_SYSTEM_TEST_FILE_SET_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

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

inline constexpr std::array kRegularFileSystemTestCases = {
    FileSystemTestCaseRecord{.is_directory = true,
                             .path = FILE_PATH_LITERAL("dir a"),
                             .data_file_size = 0,
                             .block_action = TestBlockAction::CHILD_BLOCKED},
    FileSystemTestCaseRecord{.is_directory = true,
                             .path = FILE_PATH_LITERAL("dir a/dir A"),
                             .data_file_size = 0,
                             .block_action = TestBlockAction::ALLOWED},
    FileSystemTestCaseRecord{.is_directory = true,
                             .path = FILE_PATH_LITERAL("dir a/dir d"),
                             .data_file_size = 0,
                             .block_action = TestBlockAction::CHILD_BLOCKED},
    FileSystemTestCaseRecord{.is_directory = true,
                             .path = FILE_PATH_LITERAL("dir a/dir d/dir e"),
                             .data_file_size = 0,
                             .block_action = TestBlockAction::CHILD_BLOCKED},
    FileSystemTestCaseRecord{
        .is_directory = true,
        .path = FILE_PATH_LITERAL("dir a/dir d/dir e/dir f"),
        .data_file_size = 0,
        .block_action = TestBlockAction::ALLOWED},
    FileSystemTestCaseRecord{
        .is_directory = true,
        .path = FILE_PATH_LITERAL("dir a/dir d/dir e/dir g"),
        .data_file_size = 0,
        .block_action = TestBlockAction::CHILD_BLOCKED},
    FileSystemTestCaseRecord{
        .is_directory = true,
        .path = FILE_PATH_LITERAL("dir a/dir d/dir e/dir h"),
        .data_file_size = 0,
        .block_action = TestBlockAction::BLOCKED},
    FileSystemTestCaseRecord{.is_directory = true,
                             .path = FILE_PATH_LITERAL("dir b"),
                             .data_file_size = 0,
                             .block_action = TestBlockAction::BLOCKED},
    FileSystemTestCaseRecord{.is_directory = true,
                             .path = FILE_PATH_LITERAL("dir b/dir a"),
                             .data_file_size = 0,
                             .block_action = TestBlockAction::PARENT_BLOCKED},
    FileSystemTestCaseRecord{.is_directory = true,
                             .path = FILE_PATH_LITERAL("dir c"),
                             .data_file_size = 0,
                             .block_action = TestBlockAction::ALLOWED},
    FileSystemTestCaseRecord{.is_directory = false,
                             .path = FILE_PATH_LITERAL("file 0"),
                             .data_file_size = 38,
                             .block_action = TestBlockAction::ALLOWED},
    FileSystemTestCaseRecord{.is_directory = false,
                             .path = FILE_PATH_LITERAL("file 2"),
                             .data_file_size = 60,
                             .block_action = TestBlockAction::BLOCKED},
    FileSystemTestCaseRecord{.is_directory = false,
                             .path = FILE_PATH_LITERAL("file 3"),
                             .data_file_size = 0,
                             .block_action = TestBlockAction::ALLOWED},
    FileSystemTestCaseRecord{.is_directory = false,
                             .path = FILE_PATH_LITERAL("dir a/file 0"),
                             .data_file_size = 39,
                             .block_action = TestBlockAction::ALLOWED},
    FileSystemTestCaseRecord{
        .is_directory = false,
        .path = FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 0"),
        .data_file_size = 40,
        .block_action = TestBlockAction::ALLOWED},
    FileSystemTestCaseRecord{
        .is_directory = false,
        .path = FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 1"),
        .data_file_size = 41,
        .block_action = TestBlockAction::ALLOWED},
    FileSystemTestCaseRecord{
        .is_directory = false,
        .path = FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 2"),
        .data_file_size = 42,
        .block_action = TestBlockAction::BLOCKED},
    FileSystemTestCaseRecord{
        .is_directory = false,
        .path = FILE_PATH_LITERAL("dir a/dir d/dir e/dir g/file 3"),
        .data_file_size = 43,
        .block_action = TestBlockAction::ALLOWED},
    FileSystemTestCaseRecord{
        .is_directory = false,
        .path = FILE_PATH_LITERAL("dir a/dir d/dir e/dir h/file 0"),
        .data_file_size = 44,
        .block_action = TestBlockAction::PARENT_BLOCKED},
    FileSystemTestCaseRecord{
        .is_directory = false,
        .path = FILE_PATH_LITERAL("dir a/dir d/dir e/dir h/file 1"),
        .data_file_size = 45,
        .block_action = TestBlockAction::PARENT_BLOCKED},
    FileSystemTestCaseRecord{.is_directory = false,
                             .path = FILE_PATH_LITERAL("dir b/file 0"),
                             .data_file_size = 46,
                             .block_action = TestBlockAction::PARENT_BLOCKED},
    FileSystemTestCaseRecord{.is_directory = false,
                             .path = FILE_PATH_LITERAL("dir b/file 1"),
                             .data_file_size = 47,
                             .block_action = TestBlockAction::PARENT_BLOCKED},
};

size_t GetRegularFileSystemTestCaseSize();

// Creates one file or directory specified by |record|.
void SetUpOneFileSystemTestCase(const base::FilePath& root_path,
                                const FileSystemTestCaseRecord& record);

// Creates the files and directories specified in kRegularTestCases.
void SetUpRegularFileSystemTestCases(const base::FilePath& root_path);

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_FILE_SYSTEM_TEST_FILE_SET_H_
