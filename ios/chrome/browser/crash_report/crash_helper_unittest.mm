// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/crash_helper.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"
#import "components/breadcrumbs/core/crash_reporter_breadcrumb_observer.h"
#import "ios/chrome/browser/crash_report/crash_keys_helper.h"
#import "ios/chrome/browser/crash_report/crash_report_helper.h"
#import "ios/chrome/browser/crash_report/main_thread_freeze_detector.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "ios/chrome/test/ocmock/OCMockObject+BreakpadControllerTesting.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/breakpad/breakpad/src/client/ios/BreakpadController.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const int kCrashReportCount = 3;
NSString* const kUploadedInRecoveryMode = @"uploaded_in_recovery_mode";

class BreakpadHelperTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    mock_breakpad_controller_ =
        [OCMockObject mockForClass:[BreakpadController class]];

    // Swizzle +[BreakpadController sharedInstance] to return
    // `mock_breakpad_controller_` instead of the normal singleton instance.
    id implementation_block = ^BreakpadController*(id self) {
      return mock_breakpad_controller_;
    };
    crash_helper::SyncCrashpadEnabledOnNextRun();
    breakpad_controller_shared_instance_swizzler_.reset(new ScopedBlockSwizzler(
        [BreakpadController class], @selector(sharedInstance),
        implementation_block));
  }

  void TearDown() override {
    [[mock_breakpad_controller_ stub] stop];
    crash_helper::SetEnabled(false);

    PlatformTest::TearDown();
  }

 protected:
  id mock_breakpad_controller_;
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<ScopedBlockSwizzler>
      breakpad_controller_shared_instance_swizzler_;
};

TEST_F(BreakpadHelperTest, CrashReportUserApplicationStateAllKeys) {
  // Test that the serialized dictionary does not exceed the maximum size of a
  // single breakpad record. This test should include all keys for
  // CrashReportUserApplicationState, since the whole dictionary is considered a
  // single breakpad record.
  crash_keys::SetCurrentlyInBackground(true);
  crash_keys::SetCurrentlySignedIn(true);
  crash_keys::SetMemoryWarningCount(2);
  crash_keys::SetMemoryWarningInProgress(true);
  crash_keys::SetCurrentFreeMemoryInKB(1234);
  crash_keys::SetCurrentFreeDiskInKB(12345);
  crash_keys::SetCurrentTabIsPDF(true);
  crash_keys::SetCurrentOrientation(3, 7);
  crash_keys::SetCurrentHorizontalSizeClass(2);
  crash_keys::SetCurrentUserInterfaceStyle(2);
  crash_keys::SetRegularTabCount(999);
  crash_keys::SetIncognitoTabCount(999);
  crash_keys::SetForegroundScenesCount(999);
  crash_keys::SetConnectedScenesCount(999);
  crash_keys::SetDestroyingAndRebuildingIncognitoBrowserState(true);
  crash_keys::SetGridToVisibleTabAnimation(
      @"to_view_controller", @"presenting_view_controller",
      @"presented_view_controller", @"parent_view_controller");
  crash_keys::MediaStreamPlaybackDidStart();

  // Set a max-length breadcrumbs string.
  std::string breadcrumbs(breadcrumbs::kMaxDataLength, 'A');
  breadcrumbs::CrashReporterBreadcrumbObserver::GetInstance()
      .SetPreviousSessionEvents({breadcrumbs});
}

TEST_F(BreakpadHelperTest, GetCrashReportCount) {
  [mock_breakpad_controller_ cr_expectGetCrashReportCount:kCrashReportCount];

  // Verify that crash_helper::GetPendingCrashReportCount() returns the
  // crash report count that we arranged to pass to the result block that was
  // passed to -[BreakpadController getCrashReportCount:].
  EXPECT_EQ(kCrashReportCount, crash_helper::GetPendingCrashReportCount());
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
}

TEST_F(BreakpadHelperTest, HasReportToUpload) {
  [mock_breakpad_controller_ cr_expectGetCrashReportCount:kCrashReportCount];
  EXPECT_TRUE(crash_helper::HasReportToUpload());
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);

  [mock_breakpad_controller_ cr_expectGetCrashReportCount:0];
  EXPECT_FALSE(crash_helper::HasReportToUpload());
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
}

TEST_F(BreakpadHelperTest, IsUploadingEnabled) {
  crash_helper::common::SetUserEnabledUploading(true);
  EXPECT_TRUE(crash_helper::common::UserEnabledUploading());
  crash_helper::SetEnabled(false);
  EXPECT_FALSE(crash_helper::common::UserEnabledUploading());
  [[mock_breakpad_controller_ expect] start:NO];
  crash_helper::SetEnabled(true);
  EXPECT_TRUE(crash_helper::common::UserEnabledUploading());

  crash_helper::common::SetUserEnabledUploading(false);
  EXPECT_FALSE(crash_helper::common::UserEnabledUploading());
  [[mock_breakpad_controller_ expect] stop];
  crash_helper::SetEnabled(false);
  EXPECT_FALSE(crash_helper::common::UserEnabledUploading());
  [[mock_breakpad_controller_ expect] start:NO];
  crash_helper::SetEnabled(true);
  EXPECT_TRUE(crash_helper::common::UserEnabledUploading());
}

TEST_F(BreakpadHelperTest, StartUploadingReportsInRecoveryMode) {
  // Test when crash reporter is disabled.
  crash_helper::SetEnabled(false);
  crash_helper::StartUploadingReportsInRecoveryMode();

  // Test when crash reporter is enabled.
  [[mock_breakpad_controller_ expect] start:NO];
  crash_helper::SetEnabled(true);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);

  [[mock_breakpad_controller_ expect] stop];
  [[mock_breakpad_controller_ expect]
      setParametersToAddAtUploadTime:[OCMArg checkWithBlock:^(id value) {
        return [value isKindOfClass:[NSDictionary class]] &&
                       [value[kUploadedInRecoveryMode] isEqualToString:@"yes"]
                   ? YES
                   : NO;
      }]];
  [[mock_breakpad_controller_ expect] setUploadInterval:1];
  [[mock_breakpad_controller_ expect] start:NO];
  [[mock_breakpad_controller_ expect] setUploadingEnabled:YES];
  crash_helper::StartUploadingReportsInRecoveryMode();
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
}

TEST_F(BreakpadHelperTest, RestoreDefaultConfiguration) {
  // Test when crash reporter is disabled.
  crash_helper::SetEnabled(false);
  crash_helper::RestoreDefaultConfiguration();

  // Test when crash reporter is enabled.
  [[mock_breakpad_controller_ expect] start:NO];
  crash_helper::SetEnabled(true);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);

  [[mock_breakpad_controller_ expect] stop];
  [[mock_breakpad_controller_ expect] resetConfiguration];
  [[mock_breakpad_controller_ expect] start:NO];
  [[mock_breakpad_controller_ expect] setUploadingEnabled:NO];
  crash_helper::RestoreDefaultConfiguration();
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
}

}  // namespace
