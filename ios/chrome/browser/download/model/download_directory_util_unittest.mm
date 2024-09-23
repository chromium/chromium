// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_directory_util.h"

#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForFileOperationTimeout;

using DownloadDirectoryTest = PlatformTest;

// Tests that DeleteTempDownloadsDirectory() actually deletes the directory.
TEST_F(DownloadDirectoryTest, Deletion) {
  base::test::TaskEnvironment envoronment;

  // Create a new file in downloads directory.
  base::FilePath dir;
  EXPECT_TRUE(GetTempDownloadsDirectory(&dir));
  EXPECT_TRUE(CreateDirectory(dir));
  base::FilePath file = dir.Append("file.txt");
  EXPECT_TRUE(base::WriteFile(file, ""));
  ASSERT_TRUE(base::PathExists(file));

  // Delete download directory.
  DeleteTempDownloadsDirectory();

  // Verify download directory deletion.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
    return !base::PathExists(dir);
  }));
}
