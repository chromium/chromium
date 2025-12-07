// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#import "base/ios/ios_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test fixture for DataControlsProtocolSmokeTest.
class DataControlsProtocolSmokeTest : public PlatformTest {};

// Verifies that the number of methods in the UIResponderStandardEditActions
// protocol has not changed. This acts as a smoke test to detect potential new
// vectors for data exfiltration that need to be covered by Data Controls.
TEST_F(DataControlsProtocolSmokeTest,
       TestUIResponderStandardEditActionsMethodCount) {
  if (!base::ios::IsRunningOnOrLater(18, 0, 0)) {
    GTEST_SKIP() << "All relevant UIResponderStandardEditActions "
                    "methods before iOS 18 are covered by data controls.";
  }
  Protocol* protocol = @protocol(UIResponderStandardEditActions);
  ASSERT_TRUE(protocol);

  unsigned int required_method_count = 0;
  struct objc_method_description* required_methods =
      protocol_copyMethodDescriptionList(protocol, /*isRequiredMethod=*/true,
                                         /*isInstanceMethod=*/true,
                                         &required_method_count);

  unsigned int optional_method_count = 0;
  struct objc_method_description* optional_methods =
      protocol_copyMethodDescriptionList(protocol, /*isRequiredMethod=*/false,
                                         /*isInstanceMethod=*/true,
                                         &optional_method_count);

  if (required_methods) {
    free(required_methods);
  }
  if (optional_methods) {
    free(optional_methods);
  }

  unsigned int total_method_count =
      required_method_count + optional_method_count;
  // iOS 26 introduces more methods to UIResponderStandardEditActions. None of
  // them need to be covered by Data Controls in CRWWebView.
  const unsigned int kExpectedMethodCount =
      base::ios::IsRunningOnOrLater(26, 0, 0) ? 36 : 28;
  EXPECT_EQ(kExpectedMethodCount, total_method_count)
      << "The number of methods in UIResponderStandardEditActions has changed. "
      << "Expected: " << kExpectedMethodCount
      << ", Actual: " << total_method_count
      << ". This is a Data Controls smoke test failure. Please review the "
      << "protocol definition for new methods to ensure copy/paste/cut "
      << "overrides in CRWWebView are still comprehensive and update the "
         "expected count in this test.";
}
