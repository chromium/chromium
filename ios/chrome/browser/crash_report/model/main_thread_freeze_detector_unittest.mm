// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/test/ios/wait_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using MainThreadFreezeDetectorTest = PlatformTest;

// Tests that moving a file preserves the NSFileModificationDate.
TEST_F(MainThreadFreezeDetectorTest, FileMoveSameModificationDate) {
  NSFileManager* file_manager = [[NSFileManager alloc] init];
  NSString* temp_dir = file_manager.temporaryDirectory.path;
  NSString* contents = @"test_NSFileModificationDate";
  NSString* filename1 = [[NSProcessInfo processInfo] globallyUniqueString];
  NSString* filename2 = [[NSProcessInfo processInfo] globallyUniqueString];
  NSString* original_file = [temp_dir stringByAppendingPathComponent:filename1];
  NSString* new_file = [temp_dir stringByAppendingPathComponent:filename2];
  ASSERT_TRUE([contents writeToFile:original_file
                         atomically:YES
                           encoding:NSUTF8StringEncoding
                              error:NULL]);

  NSDate* date1 = [[file_manager attributesOfItemAtPath:original_file error:nil]
      objectForKey:NSFileModificationDate];
  // Spin the run loop so, if the move below uses an updated modification date,
  // `date2` will not match (which would be a test failure).
  base::test::ios::SpinRunLoopWithMinDelay(base::Milliseconds(40));
  ASSERT_TRUE([file_manager moveItemAtPath:original_file
                                    toPath:new_file
                                     error:nil]);
  NSDate* date2 = [[file_manager attributesOfItemAtPath:new_file error:nil]
      objectForKey:NSFileModificationDate];
  ASSERT_TRUE([date1 isEqualToDate:date2]);
}

}  // namespace
