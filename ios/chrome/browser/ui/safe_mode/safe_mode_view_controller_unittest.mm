// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/safe_mode/safe_mode_view_controller.h"
#import "base/files/scoped_temp_dir.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "components/crash/core/app/crashpad.h"
#import "components/crash/core/common/reporter_running_ios.h"
#import "ios/chrome/browser/crash_report/crash_helper.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "ios/chrome/test/ocmock/OCMockObject+BreakpadControllerTesting.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/breakpad/breakpad/src/client/ios/BreakpadController.h"
#import "third_party/crashpad/crashpad/client/crash_report_database.h"
#import "third_party/crashpad/crashpad/client/crashpad_client.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const int kCrashReportCount = 2;

class SafeModeViewControllerTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    if (crash_helper::common::CanUseCrashpad()) {
      ASSERT_FALSE(crash_reporter::internal::GetCrashReportDatabase());
      ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
      database_dir_path_ = database_dir_.GetPath();
      ASSERT_TRUE(client_.StartCrashpadInProcessHandler(
          database_dir_path_, "", {},
          crashpad::CrashpadClient::
              ProcessPendingReportsObservationCallback()));
      database_ = crashpad::CrashReportDatabase::Initialize(database_dir_path_);
      crash_reporter::internal::SetCrashReportDatabaseForTesting(
          database_.get(), &database_dir_path_);

      crash_reporter::SetCrashpadRunning(true);
      return;
    }

    mock_breakpad_controller_ =
        [OCMockObject mockForClass:[BreakpadController class]];

    // Swizzle +[BreakpadController sharedInstance] to return
    // `mock_breakpad_controller_` instead of the normal singleton instance.
    id implementation_block = ^BreakpadController*(id self) {
      return mock_breakpad_controller_;
    };
    breakpad_controller_shared_instance_swizzler_.reset(new ScopedBlockSwizzler(
        [BreakpadController class], @selector(sharedInstance),
        implementation_block));
  }

  void TearDown() override {
    if (crash_helper::common::CanUseCrashpad()) {
      client_.ResetForTesting();
      crash_reporter::SetCrashpadRunning(false);
      crash_reporter::internal::SetCrashReportDatabaseForTesting(nullptr,
                                                                 nullptr);
    } else {
      [[mock_breakpad_controller_ stub] stop];
      crash_helper::SetEnabled(false);
    }

    PlatformTest::TearDown();
  }

 protected:
  crashpad::CrashpadClient client_;
  std::unique_ptr<crashpad::CrashReportDatabase> database_;
  base::FilePath database_dir_path_;
  base::ScopedTempDir database_dir_;
  base::test::TaskEnvironment task_environment;
  id mock_breakpad_controller_;
  std::unique_ptr<ScopedBlockSwizzler>
      breakpad_controller_shared_instance_swizzler_;
};

// Verify that +[SafeModeViewController hasSuggestions] returns YES if and only
// if crash reporting is enabled by the user and there are multiple crash
// reports to upload. +[SafeModeViewController hasSuggestions] does not depend
// on the value of crash_helper::IsEnabled or
// crash_helper::IsUploadingEnabled.
TEST_F(SafeModeViewControllerTest, HasSuggestions) {
  if (crash_helper::common::CanUseCrashpad()) {
    crash_helper::common::SetUserEnabledUploading(false);
    EXPECT_FALSE([SafeModeViewController hasSuggestions]);

    crash_reporter::DumpWithoutCrashing();
    crash_reporter::DumpWithoutCrashing();
    EXPECT_FALSE([SafeModeViewController hasSuggestions]);

    crash_helper::common::SetUserEnabledUploading(true);
    EXPECT_TRUE([SafeModeViewController hasSuggestions]);
    return;
  }
  // Test when crash reporter is disabled.
  crash_helper::common::SetUserEnabledUploading(false);
  EXPECT_FALSE([SafeModeViewController hasSuggestions]);

  // Test when crash reporter is enabled.
  crash_helper::common::SetUserEnabledUploading(true);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
  [mock_breakpad_controller_ cr_expectGetCrashReportCount:0];
  EXPECT_FALSE([SafeModeViewController hasSuggestions]);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);

  [mock_breakpad_controller_ cr_expectGetCrashReportCount:kCrashReportCount];
  EXPECT_TRUE([SafeModeViewController hasSuggestions]);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);

  [[mock_breakpad_controller_ expect] start:NO];
  crash_helper::SetEnabled(true);

  [[mock_breakpad_controller_ expect] setUploadingEnabled:NO];
  crash_helper::PauseBreakpadUploads();
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
  [mock_breakpad_controller_ cr_expectGetCrashReportCount:0];
  EXPECT_FALSE([SafeModeViewController hasSuggestions]);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);

  [mock_breakpad_controller_ cr_expectGetCrashReportCount:kCrashReportCount];
  EXPECT_TRUE([SafeModeViewController hasSuggestions]);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);

  // Test when crash reporter and crash report uploading are enabled.
  [[mock_breakpad_controller_ expect] setUploadingEnabled:YES];
  [[mock_breakpad_controller_ expect]
      setUploadCallback:reinterpret_cast<BreakpadUploadCompletionCallback>(
                            [OCMArg anyPointer])];
  crash_helper::UploadCrashReports();
  [mock_breakpad_controller_ cr_expectGetCrashReportCount:0];
  EXPECT_FALSE([SafeModeViewController hasSuggestions]);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);

  [mock_breakpad_controller_ cr_expectGetCrashReportCount:kCrashReportCount];
  EXPECT_TRUE([SafeModeViewController hasSuggestions]);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
}

}  // namespace
