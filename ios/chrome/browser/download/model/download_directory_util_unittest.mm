// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_directory_util.h"

#import <string_view>

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "base/test/task_environment.h"
#import "testing/platform_test.h"

using DownloadDirectoryTest = PlatformTest;

namespace {
constexpr std::string_view kPreviousSessionDirName = "previous_session";
constexpr std::string_view kOldFileName = "old_file.txt";
constexpr std::string_view kLegacyFileName = "legacy_file.txt";
constexpr std::string_view kCurrentFileName = "file.txt";
}  // namespace

// Tests that `DeleteTempDownloadsDirectory()` deletes directories and files
// from previous sessions while preserving the current session's directory.
TEST_F(DownloadDirectoryTest, Deletion) {
  base::test::TaskEnvironment task_environment;

  // Create a file in the current session's downloads directory.
  base::FilePath current_session_dir;
  ASSERT_TRUE(GetTempDownloadsDirectory(&current_session_dir));
  ASSERT_TRUE(base::CreateDirectory(current_session_dir));
  base::FilePath current_file = current_session_dir.Append(kCurrentFileName);
  ASSERT_TRUE(base::WriteFile(current_file, ""));
  ASSERT_TRUE(base::PathExists(current_file));

  // Create a directory and loose file simulating leftover downloads from
  // previous sessions.
  base::FilePath downloads_root = current_session_dir.DirName();
  base::FilePath previous_session_dir =
      downloads_root.Append(kPreviousSessionDirName);
  ASSERT_TRUE(base::CreateDirectory(previous_session_dir));
  base::FilePath old_file = previous_session_dir.Append(kOldFileName);
  ASSERT_TRUE(base::WriteFile(old_file, ""));
  ASSERT_TRUE(base::PathExists(old_file));

  base::FilePath legacy_file = downloads_root.Append(kLegacyFileName);
  ASSERT_TRUE(base::WriteFile(legacy_file, ""));
  ASSERT_TRUE(base::PathExists(legacy_file));

  // Delete previous sessions' download directories and files.
  DeleteTempDownloadsDirectory();
  base::ThreadPoolInstance::Get()->FlushForTesting();

  // Verify previous session entries are deleted while the current session file
  // is preserved.
  EXPECT_FALSE(base::PathExists(previous_session_dir));
  EXPECT_FALSE(base::PathExists(legacy_file));
  EXPECT_TRUE(base::PathExists(current_file));

  base::DeletePathRecursively(downloads_root);
}
