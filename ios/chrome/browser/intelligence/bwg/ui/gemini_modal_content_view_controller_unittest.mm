// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_modal_content_view_controller.h"

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Test fixture for GeminiModalContentViewController.
class GeminiModalContentViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    content_view_ = [[UIView alloc] init];
    mock_delegate_ =
        OCMProtocolMock(@protocol(GeminiModalContentViewControllerDelegate));
    view_controller_ = [[GeminiModalContentViewController alloc]
        initWithContentView:content_view_];
    view_controller_.delegate = mock_delegate_;
    // Hosting the view controller in a window loads its view and gives it a
    // real safe area.
    scoped_key_window_.Get().rootViewController = view_controller_;
  }

  UIBarButtonItem* CloseButton() {
    return view_controller_.navigationItem.rightBarButtonItem;
  }

  ScopedKeyWindow scoped_key_window_;
  UIView* content_view_;
  GeminiModalContentViewController* view_controller_;
  id mock_delegate_;
};

// Tests that the close button is installed in the navigation item with the
// expected accessibility identifier.
TEST_F(GeminiModalContentViewControllerTest, TestCloseButtonConfiguration) {
  EXPECT_NSEQ(kGeminiModalContentCloseButtonAccessibilityIdentifier,
              CloseButton().accessibilityIdentifier);
}

// Tests that tapping the close button notifies the delegate.
TEST_F(GeminiModalContentViewControllerTest, TestCloseButtonNotifiesDelegate) {
  OCMExpect([mock_delegate_
      geminiModalContentViewControllerDidTapClose:view_controller_]);

  [UIApplication.sharedApplication sendAction:CloseButton().action
                                           to:CloseButton().target
                                         from:CloseButton()
                                     forEvent:nil];

  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests that the navigation bar never covers the top of the content while
// the sides and bottom remain full bleed.
TEST_F(GeminiModalContentViewControllerTest, TestContentViewLayout) {
  UIView* view = view_controller_.view;
  [view layoutIfNeeded];

  EXPECT_EQ(view.safeAreaInsets.top, CGRectGetMinY(content_view_.frame));
  EXPECT_EQ(CGRectGetMinX(view.bounds), CGRectGetMinX(content_view_.frame));
  EXPECT_EQ(CGRectGetMaxX(view.bounds), CGRectGetMaxX(content_view_.frame));
  EXPECT_EQ(CGRectGetMaxY(view.bounds), CGRectGetMaxY(content_view_.frame));
}
