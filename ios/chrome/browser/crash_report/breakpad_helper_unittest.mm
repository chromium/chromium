// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breakpad_helper.h"
#import "base/test/ios/wait_util.h"
#include "ios/chrome/browser/crash_report/main_thread_freeze_detector.h"
#import "ios/chrome/test/ocmock/OCMockObject+BreakpadControllerTesting.h"
#import "ios/testing/scoped_block_swizzler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/breakpad/breakpad/src/client/ios/BreakpadController.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

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
    // |mock_breakpad_controller_| instead of the normal singleton instance.
    id implementation_block = ^BreakpadController*(id self) {
      return mock_breakpad_controller_;
    };
    breakpad_controller_shared_instance_swizzler_.reset(new ScopedBlockSwizzler(
        [BreakpadController class], @selector(sharedInstance),
        implementation_block));
  }

  void TearDown() override {
    [[mock_breakpad_controller_ stub] stop];
    breakpad_helper::SetEnabled(false);

    PlatformTest::TearDown();
  }

 protected:
  id mock_breakpad_controller_;
  std::unique_ptr<ScopedBlockSwizzler>
      breakpad_controller_shared_instance_swizzler_;
};

TEST_F(BreakpadHelperTest, CrashReportUserApplicationStateAllKeys) {
  // Test that the serialized dictionary does not exceed the maximum size of a
  // single breakpad record. This test should include all keys for
  // CrashReportUserApplicationState, since the whole dictionary is considered a
  // single breakpad record.
  breakpad_helper::SetCurrentOrientation(3, 7);
  breakpad_helper::SetCurrentHorizontalSizeClass(2);
  breakpad_helper::SetCurrentUserInterfaceStyle(2);
  breakpad_helper::SetRegularTabCount(999);
  breakpad_helper::SetIncognitoTabCount(999);
  breakpad_helper::SetDestroyingAndRebuildingIncognitoBrowserState(true);
  breakpad_helper::MediaStreamPlaybackDidStart();
  breakpad_helper::SetCurrentTabIsPDF(true);
  breakpad_helper::SetCurrentlySignedIn(true);
}

TEST_F(BreakpadHelperTest, GetCrashReportCount) {
  [mock_breakpad_controller_ cr_expectGetCrashReportCount:kCrashReportCount];

  // Verify that breakpad_helper::GetCrashReportCount() returns the
  // crash report count that we arranged to pass to the result block that was
  // passed to -[BreakpadController getCrashReportCount:].
  EXPECT_EQ(kCrashReportCount, breakpad_helper::GetCrashReportCount());
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
}

TEST_F(BreakpadHelperTest, HasReportToUpload) {
  [mock_breakpad_controller_ cr_expectGetCrashReportCount:kCrashReportCount];
  EXPECT_TRUE(breakpad_helper::HasReportToUpload());
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);

  [mock_breakpad_controller_ cr_expectGetCrashReportCount:0];
  EXPECT_FALSE(breakpad_helper::HasReportToUpload());
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
}

TEST_F(BreakpadHelperTest, IsUploadingEnabled) {
  breakpad_helper::SetUserEnabledUploading(true);
  EXPECT_TRUE(breakpad_helper::UserEnabledUploading());
  breakpad_helper::SetEnabled(false);
  EXPECT_TRUE(breakpad_helper::UserEnabledUploading());
  [[mock_breakpad_controller_ expect] start:NO];
  breakpad_helper::SetEnabled(true);
  EXPECT_TRUE(breakpad_helper::UserEnabledUploading());

  breakpad_helper::SetUserEnabledUploading(false);
  EXPECT_FALSE(breakpad_helper::UserEnabledUploading());
  [[mock_breakpad_controller_ expect] stop];
  breakpad_helper::SetEnabled(false);
  EXPECT_FALSE(breakpad_helper::UserEnabledUploading());
  [[mock_breakpad_controller_ expect] start:NO];
  breakpad_helper::SetEnabled(true);
  EXPECT_FALSE(breakpad_helper::UserEnabledUploading());
}

TEST_F(BreakpadHelperTest, StartUploadingReportsInRecoveryMode) {
  // Test when crash reporter is disabled.
  breakpad_helper::SetEnabled(false);
  breakpad_helper::StartUploadingReportsInRecoveryMode();

  // Test when crash reporter is enabled.
  [[mock_breakpad_controller_ expect] start:NO];
  breakpad_helper::SetEnabled(true);
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
  breakpad_helper::StartUploadingReportsInRecoveryMode();
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
}

TEST_F(BreakpadHelperTest, RestoreDefaultConfiguration) {
  // Test when crash reporter is disabled.
  breakpad_helper::SetEnabled(false);
  breakpad_helper::RestoreDefaultConfiguration();

  // Test when crash reporter is enabled.
  [[mock_breakpad_controller_ expect] start:NO];
  breakpad_helper::SetEnabled(true);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);

  [[mock_breakpad_controller_ expect] stop];
  [[mock_breakpad_controller_ expect] resetConfiguration];
  [[mock_breakpad_controller_ expect] start:NO];
  [[mock_breakpad_controller_ expect] setUploadingEnabled:NO];
  breakpad_helper::RestoreDefaultConfiguration();
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
}

}  // namespace
