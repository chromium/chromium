// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_util.h"

#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_positioner.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/web/common/uikit_ui_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface TestModalPositioner : NSObject <InfobarModalPositioner>
@end

@implementation TestModalPositioner

- (CGFloat)modalHeightForWidth:(CGFloat)width {
  return 350;
}

@end

using OverlayPresentationUtilTest = PlatformTest;

// Tests that the frame returned by `ContainedModalFrameThatFit` fits in the
// given view.
TEST_F(OverlayPresentationUtilTest, TestContainedModalFrameThatFit) {
  TestModalPositioner* positioner = [[TestModalPositioner alloc] init];

  CGRect frame = CGRectMake(0, 0, 500, 500);
  UITextView* text_view = CreateUITextViewWithTextKit1();
  text_view.frame = frame;
  text_view.text = @"test";

  // The text_view must be added to the window so that its safe area is not
  // null.
  [GetAnyKeyWindow() addSubview:text_view];

  CGRect new_frame = ContainedModalFrameThatFit(positioner, text_view);

  EXPECT_TRUE(text_view.frame.size.width > new_frame.size.width);
  EXPECT_TRUE(text_view.frame.size.height > new_frame.size.height);

  EXPECT_EQ(new_frame.size.width, 394);
  EXPECT_EQ(new_frame.size.height, 350);

  [text_view removeFromSuperview];
}
