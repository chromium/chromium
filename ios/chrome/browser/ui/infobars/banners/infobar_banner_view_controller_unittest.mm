// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"

#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface InfobarBannerViewController (Testing)
@property(nonatomic, weak) UIButton* infobarButton;
@property(nonatomic, strong) UILabel* titleLabel;
@property(nonatomic, strong) UILabel* subTitleLabel;
@end

// Test fixture for testing InfobarBannerViewController class.
class InfobarBannerViewControllerTest : public PlatformTest {
 protected:
  InfobarBannerViewControllerTest()
      : banner_delegate_(OCMProtocolMock(@protocol(InfobarBannerDelegate))),
        view_controller_([[InfobarBannerViewController alloc]
            initWithDelegate:banner_delegate_
               presentsModal:YES
                        type:InfobarType::kInfobarTypeConfirm]) {}
  id banner_delegate_;
  InfobarBannerViewController* view_controller_;
  ScopedKeyWindow scoped_key_window_;
};

TEST_F(InfobarBannerViewControllerTest, TestBannerButtonPressed) {
  OCMExpect([banner_delegate_ bannerInfobarButtonWasPressed:[OCMArg any]]);
  // Add view_controller_ to the UI Hierarchy to make sure views are created and
  // retained correctly.
  [scoped_key_window_.Get() setRootViewController:view_controller_];
  [view_controller_.infobarButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(banner_delegate_);
}

TEST_F(InfobarBannerViewControllerTest, TestTextConfiguration) {
  NSString* const kTitle = @"title";
  NSString* const kSubtitleText = @"subtitle";
  NSString* const kButtonText = @"buttonText";
  [view_controller_ setTitleText:kTitle];
  [view_controller_ setSubtitleText:kSubtitleText];
  [view_controller_ setButtonText:kButtonText];
  // Add view_controller_ to the UI Hierarchy to make sure views are created and
  // retained correctly.
  [scoped_key_window_.Get() setRootViewController:view_controller_];
  ASSERT_EQ(view_controller_.titleLabel.text, kTitle);
  ASSERT_EQ(view_controller_.subTitleLabel.text, kSubtitleText);
  ASSERT_EQ(view_controller_.infobarButton.titleLabel.text, kButtonText);
}

TEST_F(InfobarBannerViewControllerTest, TestSubtitleLabelHidden) {
  view_controller_.titleText = @"title";
  // Add view_controller_ to the UI Hierarchy to make sure views are created and
  // retained correctly.
  [scoped_key_window_.Get() setRootViewController:view_controller_];
  ASSERT_EQ(view_controller_.titleLabel.text, @"title");
  ASSERT_TRUE(view_controller_.subTitleLabel.hidden);
}
