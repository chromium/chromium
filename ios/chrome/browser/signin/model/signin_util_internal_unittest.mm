// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/signin_util_internal.h"

#import <UIKit/UIKit.h>

#import "base/files/file.h"
#import "base/files/file_util.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
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

  void ExpectSentinelFile(NSURL* url, BOOL is_excluded_from_backup) {
    std::string url_description = base::SysNSStringToUTF8([url description]);
    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:url.path])
        << url_description;
    NSError* error = nil;
    id resource_value;
    BOOL success = [url getResourceValue:&resource_value
                                  forKey:NSURLIsExcludedFromBackupKey
                                   error:&error];
    ASSERT_TRUE(success) << url_description;
    ASSERT_EQ(nil, error) << url_description;
    EXPECT_NE(nil, resource_value) << url_description;
    EXPECT_EQ(is_excluded_from_backup, [resource_value boolValue])
        << url_description;
  }

  void ExpectSentinelThatIsBackedUp() {
    NSURL* url = GetSentinelThatIsBackedUpURLPath();
    ExpectSentinelFile(url, /*is_excluded_from_backup=*/NO);
  }

  void ExpectSentinelThatIsNotBackedUp() {
    NSURL* url = GetSentinelThatIsNotBackedUpURLPath();
    ExpectSentinelFile(url, /*is_excluded_from_backup=*/YES);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
};

// Tests the result of IsFirstSessionAfterDeviceRestoreInternal(), when no
// sentinel exists.
TEST_F(SigninUtilInternalTest,
       IsFirstSessionAfterDeviceRestoreInternalNoSentinelFile) {
  {
    base::RunLoop run_loop;
    signin::RestoreData restore_data =
        LoadDeviceRestoreDataInternal(run_loop.QuitClosure());
    EXPECT_EQ(signin::Tribool::kUnknown,
              restore_data.is_first_session_after_device_restore);
    // Device restore is unknown, so there is no timestamp.
    EXPECT_FALSE(restore_data.last_restore_timestamp.has_value());
    run_loop.Run();
    ExpectSentinelThatIsBackedUp();
    ExpectSentinelThatIsNotBackedUp();
  }
  // Simulate the next run.
  {
    base::RunLoop run_loop;
    signin::RestoreData restore_data =
        LoadDeviceRestoreDataInternal(run_loop.QuitClosure());
    run_loop.Run();
    // After both sentinel files are created, there is still no device restore
    // detected.
    EXPECT_EQ(signin::Tribool::kFalse,
              restore_data.is_first_session_after_device_restore);
    EXPECT_FALSE(restore_data.last_restore_timestamp.has_value());
  }
}

// Tests the result of IsFirstSessionAfterDeviceRestoreInternal(), when only the
// backed up sentinel file exist.
TEST_F(SigninUtilInternalTest,
       IsFirstSessionAfterDeviceRestoreInternalAfterRestore) {
  NSURL* url = GetSentinelThatIsBackedUpURLPath();
  [[NSFileManager defaultManager] createFileAtPath:url.path
                                          contents:nil
                                        attributes:nil];
  {
    base::RunLoop run_loop;
    signin::RestoreData restore_data =
        LoadDeviceRestoreDataInternal(run_loop.QuitClosure());
    EXPECT_EQ(signin::Tribool::kTrue,
              restore_data.is_first_session_after_device_restore);
    // First run after a device restore, so there is no timestamp.
    EXPECT_FALSE(restore_data.last_restore_timestamp.has_value());
    run_loop.Run();
    ExpectSentinelThatIsBackedUp();
    ExpectSentinelThatIsNotBackedUp();
  }
  // Simulate the next run.
  {
    base::RunLoop run_loop;
    signin::RestoreData restore_data =
        LoadDeviceRestoreDataInternal(run_loop.QuitClosure());
    run_loop.Run();
    // This is not the first session after the device restore.
    EXPECT_EQ(signin::Tribool::kFalse,
              restore_data.is_first_session_after_device_restore);
    // There is restore timestamp since there was a device restore in a previous
    // run.
    EXPECT_TRUE(restore_data.last_restore_timestamp.has_value());
  }
}

// Tests the result of IsFirstSessionAfterDeviceRestoreInternal(), when only the
// not-backed-up sentinel file exist.
TEST_F(SigninUtilInternalTest,
       IsFirstSessionAfterDeviceRestoreInternalAfterUnexpectedSentinel) {
  NSURL* url = GetSentinelThatIsNotBackedUpURLPath();
  [[NSFileManager defaultManager] createFileAtPath:url.path
                                          contents:nil
                                        attributes:nil];
  {
    base::RunLoop run_loop;
    signin::RestoreData restore_data =
        LoadDeviceRestoreDataInternal(run_loop.QuitClosure());
    EXPECT_EQ(signin::Tribool::kUnknown,
              restore_data.is_first_session_after_device_restore);
    // Device restore is unknown, so there is no timestamp.
    EXPECT_FALSE(restore_data.last_restore_timestamp.has_value());
    run_loop.Run();
    ExpectSentinelThatIsBackedUp();
  }
  // Simulate the next run.
  {
    base::RunLoop run_loop;
    signin::RestoreData restore_data =
        LoadDeviceRestoreDataInternal(run_loop.QuitClosure());
    run_loop.Run();
    // This is not the first session after the device restore.
    EXPECT_EQ(signin::Tribool::kFalse,
              restore_data.is_first_session_after_device_restore);
    // No device restore happened in previous run.
    EXPECT_FALSE(restore_data.last_restore_timestamp.has_value());
  }
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
  {
    base::RunLoop run_loop;
    signin::RestoreData restore_data =
        LoadDeviceRestoreDataInternal(run_loop.QuitClosure());
    EXPECT_EQ(signin::Tribool::kFalse,
              restore_data.is_first_session_after_device_restore);
    EXPECT_TRUE(restore_data.last_restore_timestamp.has_value());
    run_loop.Run();
    // The device restore happened on the create timestamp of the not backed up
    // create date.
    EXPECT_NSEQ(not_backed_up_creation_date,
                restore_data.last_restore_timestamp->ToNSDate());
  }
  // Simulate the next run.
  {
    base::RunLoop run_loop;
    signin::RestoreData restore_data =
        LoadDeviceRestoreDataInternal(run_loop.QuitClosure());
    run_loop.Run();
    // Expect the values to be the same.
    EXPECT_EQ(signin::Tribool::kFalse,
              restore_data.is_first_session_after_device_restore);
    EXPECT_TRUE(restore_data.last_restore_timestamp.has_value());
    EXPECT_NSEQ(not_backed_up_creation_date,
                restore_data.last_restore_timestamp->ToNSDate());
  }
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
  {
    base::RunLoop run_loop;
    signin::RestoreData restore_data =
        LoadDeviceRestoreDataInternal(run_loop.QuitClosure());
    EXPECT_EQ(signin::Tribool::kFalse,
              restore_data.is_first_session_after_device_restore);
    EXPECT_FALSE(restore_data.last_restore_timestamp.has_value());
    run_loop.Run();
  }
  // Simulate the next run.
  {
    base::RunLoop run_loop;
    signin::RestoreData restore_data =
        LoadDeviceRestoreDataInternal(run_loop.QuitClosure());
    run_loop.Run();
    // Expect the values to be the same.
    EXPECT_EQ(signin::Tribool::kFalse,
              restore_data.is_first_session_after_device_restore);
    EXPECT_FALSE(restore_data.last_restore_timestamp.has_value());
  }
}
