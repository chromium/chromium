// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/safe_mode/safe_mode_view_controller.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/crash_report/breakpad_helper.h"
#import "ios/chrome/browser/crash_report/main_thread_freeze_detector.h"
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

const int kCrashReportCount = 2;

class SafeModeViewControllerTest : public PlatformTest {
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

// Verify that +[SafeModeViewController hasSuggestions] returns YES if and only
// if crash reporting is enabled by the user and there are multiple crash
// reports to upload. +[SafeModeViewController hasSuggestions] does not depend
// on the value of breakpad_helper::IsEnabled or
// breakpad_helper::IsUploadingEnabled.
TEST_F(SafeModeViewControllerTest, HasSuggestions) {
  // Test when crash reporter is disabled.
  breakpad_helper::SetUserEnabledUploading(false);
  EXPECT_FALSE([SafeModeViewController hasSuggestions]);

  breakpad_helper::SetUploadingEnabled(false);
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      // Calling SetUploadingEnabled() for the first time kicks off several
      // asynchronous calls that ultimately result in MainThreadFreezeDetector's
      // |-canUploadBreakpadCrashReports| being flipped to YES.  Subsequent
      // calls will perform synchronously after |canUploadBreakpadCrashReports|
      // is YES.  The OCMock verification calls below expect these selectors to
      // be called synchronously, so wait until |canUploadBreakpadCrashReports|
      // is YES before continuing the test.
      // TODO(crbug.com/931826): Remove timing assumptions for the OCMock
      // verification calls below.
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return [MainThreadFreezeDetector sharedInstance]
            .canUploadBreakpadCrashReports;
      }));
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
  EXPECT_FALSE([SafeModeViewController hasSuggestions]);

  breakpad_helper::SetUploadingEnabled(true);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
  EXPECT_FALSE([SafeModeViewController hasSuggestions]);

  // Test when crash reporter is enabled.
  breakpad_helper::SetUserEnabledUploading(true);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
  [mock_breakpad_controller_ cr_expectGetCrashReportCount:0];
  EXPECT_FALSE([SafeModeViewController hasSuggestions]);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);

  [mock_breakpad_controller_ cr_expectGetCrashReportCount:kCrashReportCount];
  EXPECT_TRUE([SafeModeViewController hasSuggestions]);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);

  [[mock_breakpad_controller_ expect] start:NO];
  breakpad_helper::SetEnabled(true);

  [[mock_breakpad_controller_ expect] setUploadingEnabled:NO];
  breakpad_helper::SetUploadingEnabled(false);
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
  breakpad_helper::SetUploadingEnabled(true);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);

  [mock_breakpad_controller_ cr_expectGetCrashReportCount:0];
  EXPECT_FALSE([SafeModeViewController hasSuggestions]);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);

  [mock_breakpad_controller_ cr_expectGetCrashReportCount:kCrashReportCount];
  EXPECT_TRUE([SafeModeViewController hasSuggestions]);
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
}

}  // namespace
