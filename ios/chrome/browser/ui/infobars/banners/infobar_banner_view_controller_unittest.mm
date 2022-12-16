// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"

#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  ASSERT_NSEQ(view_controller_.titleLabel.text, kTitle);
  ASSERT_NSEQ(view_controller_.subTitleLabel.text, kSubtitleText);
  ASSERT_NSEQ(view_controller_.infobarButton.titleLabel.text, kButtonText);
}

TEST_F(InfobarBannerViewControllerTest, TestSubtitleLabelHidden) {
  view_controller_.titleText = @"title";
  // Add view_controller_ to the UI Hierarchy to make sure views are created and
  // retained correctly.
  [scoped_key_window_.Get() setRootViewController:view_controller_];
  ASSERT_NSEQ(view_controller_.titleLabel.text, @"title");
  ASSERT_TRUE(view_controller_.subTitleLabel.hidden);
}

TEST_F(InfobarBannerViewControllerTest, TestAddCustomStyleToSubtitleRange) {
  NSString* const kSubtitleText = @"BoldItalic";
  NSRange expectedBoldRange = [kSubtitleText rangeOfString:@"Bold"];
  NSRange expectedItalicRange = [kSubtitleText rangeOfString:@"Italic"];
  [view_controller_ setSubtitleText:kSubtitleText];

  [view_controller_ addCustomStyle:UIFontDescriptorTraitBold
                   toSubtitleRange:expectedBoldRange];
  [view_controller_ addCustomStyle:UIFontDescriptorTraitItalic
                   toSubtitleRange:expectedItalicRange];
  [scoped_key_window_.Get() setRootViewController:view_controller_];

  NSRange maxSearchRange = NSMakeRange(0, [kSubtitleText length]);
  NSRange range1;
  UIFont* font1 = [view_controller_.subTitleLabel.attributedText
                  attribute:NSFontAttributeName
                    atIndex:0
      longestEffectiveRange:&range1
                    inRange:maxSearchRange];
  UIFontDescriptorSymbolicTraits traits1 =
      [[font1 fontDescriptor] symbolicTraits];
  EXPECT_TRUE(NSEqualRanges(range1, expectedBoldRange));
  EXPECT_TRUE(traits1 & UIFontDescriptorTraitBold);
  EXPECT_FALSE(traits1 & UIFontDescriptorTraitItalic);
  NSRange range2;
  UIFont* font2 = [view_controller_.subTitleLabel.attributedText
                  attribute:NSFontAttributeName
                    atIndex:expectedItalicRange.location
      longestEffectiveRange:&range2
                    inRange:maxSearchRange];
  UIFontDescriptorSymbolicTraits traits2 =
      [[font2 fontDescriptor] symbolicTraits];
  EXPECT_TRUE(NSEqualRanges(range2, expectedItalicRange));
  EXPECT_FALSE(traits2 & UIFontDescriptorTraitBold);
  EXPECT_TRUE(traits2 & UIFontDescriptorTraitItalic);
}
