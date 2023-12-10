// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_util_internal.h"

#import <UIKit/UIKit.h>

#import "base/files/file.h"
#import "base/files/file_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "testing/platform_test.h"

class SigninUtilInternalTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    NSError* error = nil;
    NSURL* url = GetSentinelThatIsBackedUpURLPath();
    if ([[NSFileManager defaultManager] fileExistsAtPath:[url path]]) {
      [[NSFileManager defaultManager] removeItemAtURL:url error:&error];
      ASSERT_EQ(nil, error);
    }
    url = GetSentinelThatIsNotBackedUpURLPath();
    if ([[NSFileManager defaultManager] fileExistsAtPath:[url path]]) {
      [[NSFileManager defaultManager] removeItemAtURL:url error:&error];
      ASSERT_EQ(nil, error);
    }
  }

  NSURL* GetSentinelThatIsBackedUpURLPath() {
    const base::FilePath path = PathForSentinel(kSentinelThatIsBackedUp);
    NSString* path_string = base::SysUTF8ToNSString(path.value());
    return [NSURL fileURLWithPath:path_string];
  }

  NSURL* GetSentinelThatIsNotBackedUpURLPath() {
    const base::FilePath path = PathForSentinel(kSentinelThatIsNotBackedUp);
    NSString* path_string = base::SysUTF8ToNSString(path.value());
    return [NSURL fileURLWithPath:path_string];
  }

  void WaitAndExpectSentinelThatIsBackedUp() {
    NSURL* url = GetSentinelThatIsBackedUpURLPath();
    ConditionBlock condition = ^() {
      BOOL sentinel_exist =
          [[NSFileManager defaultManager] fileExistsAtPath:[url path]];
      return sentinel_exist;
    };
    bool wait_success = base::test::ios::WaitUntilConditionOrTimeout(
        base::Seconds(5), condition);
    EXPECT_TRUE(wait_success);
    NSError* error = nil;
    id resource_value;
    BOOL success = [url getResourceValue:&resource_value
                                  forKey:NSURLIsExcludedFromBackupKey
                                   error:&error];
    ASSERT_TRUE(success);
    ASSERT_EQ(nil, error);
    EXPECT_NE(nil, resource_value);
    EXPECT_FALSE([resource_value boolValue]);
  }

  void WaitAndExpectSentinelThatIsNotBackedUp() {
    NSURL* url = GetSentinelThatIsNotBackedUpURLPath();
    ConditionBlock condition = ^() {
      BOOL sentinel_exist =
          [[NSFileManager defaultManager] fileExistsAtPath:[url path]];
      return sentinel_exist;
    };
    bool wait_success = base::test::ios::WaitUntilConditionOrTimeout(
        base::Seconds(5), condition);
    EXPECT_TRUE(wait_success);
    __block id resource_value = nil;
    __block NSError* error = nil;
    condition = ^bool() {
      [url getResourceValue:&resource_value
                     forKey:NSURLIsExcludedFromBackupKey
                      error:&error];
      return resource_value != nil && [resource_value boolValue];
    };
    wait_success = base::test::ios::WaitUntilConditionOrTimeout(
        base::Seconds(5), condition);
    ASSERT_EQ(nil, error);
    EXPECT_TRUE(wait_success);
  }
};

// Tests the result of IsFirstSessionAfterDeviceRestoreInternal(), when no
// sentinel exists.
TEST_F(SigninUtilInternalTest,
       IsFirstSessionAfterDeviceRestoreInternalNoSentinelFile) {
  EXPECT_EQ(signin::Tribool::kUnknown,
            IsFirstSessionAfterDeviceRestoreInternal());
  WaitAndExpectSentinelThatIsBackedUp();
  WaitAndExpectSentinelThatIsNotBackedUp();
}

// Tests the result of IsFirstSessionAfterDeviceRestoreInternal(), when only the
// backed up sentinel file exist.
TEST_F(SigninUtilInternalTest,
       IsFirstSessionAfterDeviceRestoreInternalAfterRestore) {
  NSURL* url = GetSentinelThatIsBackedUpURLPath();
  [[NSFileManager defaultManager] createFileAtPath:[url path]
                                          contents:nil
                                        attributes:nil];
  EXPECT_EQ(signin::Tribool::kTrue, IsFirstSessionAfterDeviceRestoreInternal());
  WaitAndExpectSentinelThatIsBackedUp();
  WaitAndExpectSentinelThatIsNotBackedUp();
}

// Tests the result of IsFirstSessionAfterDeviceRestoreInternal(), when only the
// not-backed-up sentinel file exist.
TEST_F(SigninUtilInternalTest,
       IsFirstSessionAfterDeviceRestoreInternalAfterUnexpectedSentinel) {
  NSURL* url = GetSentinelThatIsNotBackedUpURLPath();
  [[NSFileManager defaultManager] createFileAtPath:[url path]
                                          contents:nil
                                        attributes:nil];
  EXPECT_EQ(signin::Tribool::kUnknown,
            IsFirstSessionAfterDeviceRestoreInternal());
}

// Tests the result of IsFirstSessionAfterDeviceRestoreInternal(), when all
// sentinel exists.
TEST_F(SigninUtilInternalTest,
       IsFirstSessionAfterDeviceRestoreInternalWithoutRestore) {
  NSURL* url = GetSentinelThatIsBackedUpURLPath();
  [[NSFileManager defaultManager] createFileAtPath:[url path]
                                          contents:nil
                                        attributes:nil];
  url = GetSentinelThatIsNotBackedUpURLPath();
  [[NSFileManager defaultManager] createFileAtPath:[url path]
                                          contents:nil
                                        attributes:nil];
  EXPECT_EQ(signin::Tribool::kFalse,
            IsFirstSessionAfterDeviceRestoreInternal());
}
