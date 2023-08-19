// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/text_badge_view.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using TextBadgeViewTest = PlatformTest;

// Test that the `text` property is set during initialization.
TEST_F(TextBadgeViewTest, CreateBadge) {
  TextBadgeView* badge = [[TextBadgeView alloc] initWithText:@"text"];
  EXPECT_NSEQ(@"text", badge.text);
}

// Test setting the `text` property.
TEST_F(TextBadgeViewTest, SetText) {
  TextBadgeView* badge = [[TextBadgeView alloc] initWithText:@"text 1"];
  [badge setText:@"text 2"];
  EXPECT_NSEQ(@"text 2", badge.text);
}

// Test that the accessibility label matches the display text.
TEST_F(TextBadgeViewTest, Accessibility) {
  TextBadgeView* badge = [[TextBadgeView alloc] initWithText:@"display"];
  UIView* superview = [[UIView alloc] initWithFrame:CGRectZero];
  [superview addSubview:badge];
  EXPECT_NSEQ(@"display", badge.accessibilityLabel);
}
