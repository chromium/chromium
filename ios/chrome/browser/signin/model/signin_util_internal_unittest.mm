// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_util_internal.h"

#import <UIKit/UIKit.h>

#import "base/files/file.h"
#import "base/files/file_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "testing/gtest_mac.h"
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
  signin::RestoreData restore_data = LoadDeviceRestoreDataInternal();
  EXPECT_EQ(signin::Tribool::kUnknown,
            restore_data.is_first_session_after_device_restore);
  // Device restore is unknown, so there is no timestamp.
  EXPECT_FALSE(restore_data.last_restore_timestamp.has_value());
  WaitAndExpectSentinelThatIsBackedUp();
  WaitAndExpectSentinelThatIsNotBackedUp();
  // Simulate the next run.
  restore_data = LoadDeviceRestoreDataInternal();
  // After both sentinel files are created, there is still no device restore
  // detected.
  EXPECT_EQ(signin::Tribool::kFalse,
            restore_data.is_first_session_after_device_restore);
  EXPECT_FALSE(restore_data.last_restore_timestamp.has_value());
}

// Tests the result of IsFirstSessionAfterDeviceRestoreInternal(), when only the
// backed up sentinel file exist.
TEST_F(SigninUtilInternalTest,
       IsFirstSessionAfterDeviceRestoreInternalAfterRestore) {
  NSURL* url = GetSentinelThatIsBackedUpURLPath();
  [[NSFileManager defaultManager] createFileAtPath:[url path]
                                          contents:nil
                                        attributes:nil];
  signin::RestoreData restore_data = LoadDeviceRestoreDataInternal();
  EXPECT_EQ(signin::Tribool::kTrue,
            restore_data.is_first_session_after_device_restore);
  // First run after a device restore, so there is no timestamp.
  EXPECT_FALSE(restore_data.last_restore_timestamp.has_value());
  WaitAndExpectSentinelThatIsBackedUp();
  WaitAndExpectSentinelThatIsNotBackedUp();
  // Simulate the next run.
  restore_data = LoadDeviceRestoreDataInternal();
  // This is not the first session after the device restore.
  EXPECT_EQ(signin::Tribool::kFalse,
            restore_data.is_first_session_after_device_restore);
  // There is restore timestamp since there was a device restore in a previous
  // run.
  EXPECT_TRUE(restore_data.last_restore_timestamp.has_value());
}

// Tests the result of IsFirstSessionAfterDeviceRestoreInternal(), when only the
// not-backed-up sentinel file exist.
TEST_F(SigninUtilInternalTest,
       IsFirstSessionAfterDeviceRestoreInternalAfterUnexpectedSentinel) {
  NSURL* url = GetSentinelThatIsNotBackedUpURLPath();
  [[NSFileManager defaultManager] createFileAtPath:[url path]
                                          contents:nil
                                        attributes:nil];
  signin::RestoreData restore_data = LoadDeviceRestoreDataInternal();
  EXPECT_EQ(signin::Tribool::kUnknown,
            restore_data.is_first_session_after_device_restore);
  // Device restore is unknown, so there is no timestamp.
  EXPECT_FALSE(restore_data.last_restore_timestamp.has_value());
  WaitAndExpectSentinelThatIsBackedUp();
  // Simulate the next run.
  restore_data = LoadDeviceRestoreDataInternal();
  // This is not the first session after the device restore.
  EXPECT_EQ(signin::Tribool::kFalse,
            restore_data.is_first_session_after_device_restore);
  // No device restore happened in previous run.
  EXPECT_FALSE(restore_data.last_restore_timestamp.has_value());
}

// Tests the result of IsFirstSessionAfterDeviceRestoreInternal(), when all
// sentinel exists. The backed up sentinel file is created before the not backed
// up sentinel file, which means that a device restore happened in a previous
// run.
TEST_F(SigninUtilInternalTest,
       IsFirstSessionAfterDeviceRestoreInternalWithoutRestore) {
  NSURL* backed_up_url = GetSentinelThatIsBackedUpURLPath();
  NSDate* backed_up_creation_date =
      [NSDate dateWithTimeIntervalSinceReferenceDate:42];
  NSDictionary* backed_up_attributes =
      @{NSFileCreationDate : backed_up_creation_date};
  [[NSFileManager defaultManager] createFileAtPath:backed_up_url.path
                                          contents:nil
                                        attributes:backed_up_attributes];
  NSURL* not_backed_up_url = GetSentinelThatIsNotBackedUpURLPath();
  NSDate* not_backed_up_creation_date =
      [NSDate dateWithTimeIntervalSinceReferenceDate:4200];
  NSDictionary* not_backed_up_attributes =
      @{NSFileCreationDate : not_backed_up_creation_date};
  [[NSFileManager defaultManager] createFileAtPath:not_backed_up_url.path
                                          contents:nil
                                        attributes:not_backed_up_attributes];
  signin::RestoreData restore_data = LoadDeviceRestoreDataInternal();
  EXPECT_EQ(signin::Tribool::kFalse,
            restore_data.is_first_session_after_device_restore);
  EXPECT_TRUE(restore_data.last_restore_timestamp.has_value());
  // The device restore happened on the create timestamp of the not backed up
  // create date.
  EXPECT_NSEQ(not_backed_up_creation_date,
              restore_data.last_restore_timestamp->ToNSDate());
  // Simulate the next run.
  restore_data = LoadDeviceRestoreDataInternal();
  // Expect the values to be the same.
  EXPECT_EQ(signin::Tribool::kFalse,
            restore_data.is_first_session_after_device_restore);
  EXPECT_TRUE(restore_data.last_restore_timestamp.has_value());
  EXPECT_NSEQ(not_backed_up_creation_date,
              restore_data.last_restore_timestamp->ToNSDate());
}

// Tests the result of IsFirstSessionAfterDeviceRestoreInternal(), when all
// sentinel exists. The backed up sentinel file is created after the not backed
// up sentinel file, which means that no restore happened
TEST_F(SigninUtilInternalTest,
       IsFirstSessionAfterDeviceRestoreInternalWithPreviousRestore) {
  NSURL* not_backed_up_url = GetSentinelThatIsNotBackedUpURLPath();
  NSDate* not_backed_up_creation_date =
      [NSDate dateWithTimeIntervalSinceReferenceDate:42];
  NSDictionary* not_backed_up_attributes =
      @{NSFileCreationDate : not_backed_up_creation_date};
  [[NSFileManager defaultManager] createFileAtPath:not_backed_up_url.path
                                          contents:nil
                                        attributes:not_backed_up_attributes];
  NSURL* backed_up_url = GetSentinelThatIsBackedUpURLPath();
  NSDate* backed_up_creation_date =
      [NSDate dateWithTimeIntervalSinceReferenceDate:4200];
  NSDictionary* backed_up_attributes =
      @{NSFileCreationDate : backed_up_creation_date};
  [[NSFileManager defaultManager] createFileAtPath:backed_up_url.path
                                          contents:nil
                                        attributes:backed_up_attributes];
  signin::RestoreData restore_data = LoadDeviceRestoreDataInternal();
  EXPECT_EQ(signin::Tribool::kFalse,
            restore_data.is_first_session_after_device_restore);
  EXPECT_FALSE(restore_data.last_restore_timestamp.has_value());
  // Simulate the next run.
  restore_data = LoadDeviceRestoreDataInternal();
  // Expect the values to be the same.
  EXPECT_EQ(signin::Tribool::kFalse,
            restore_data.is_first_session_after_device_restore);
  EXPECT_FALSE(restore_data.last_restore_timestamp.has_value());
}
