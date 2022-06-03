// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/download_directory_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#import "base/test/ios/wait_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  EXPECT_EQ(0, WriteFile(file, "", 0));
  ASSERT_TRUE(base::PathExists(file));

  // Delete download directory.
  DeleteTempDownloadsDirectory();

  // Verify download directory deletion.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
    return !base::PathExists(dir);
  }));
}
