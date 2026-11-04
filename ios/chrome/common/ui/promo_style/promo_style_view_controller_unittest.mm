// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface PromoStyleViewController (Testing)
@property(nonatomic, strong, readonly) UITextView* disclaimerView;
@property(nonatomic, strong, readonly)
    UIActivityIndicatorView* contentLoadingSpinner;
- (void)updateViewsForInitialScrollState;
- (void)handleDidReachBottomOfContent;
@end

namespace {

class PromoStyleViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ButtonStackConfiguration* configuration =
        [[ButtonStackConfiguration alloc] init];
    configuration.primaryActionString = @"Primary";
    configuration.secondaryActionString = @"Secondary";
    view_controller_ =
        [[PromoStyleViewController alloc] initWithConfiguration:configuration];
    view_controller_.titleText = @"Title";
    view_controller_.subtitleText = @"Subtitle";
    view_controller_.disclaimerText = @"Disclaimer";
    view_controller_.disclaimerURLs = @[];
  }

  PromoStyleViewController* view_controller_ = nil;
};

// Tests that `contentLoading` hides the subtitle, specific content view,
// disclaimer, and action buttons while keeping the banner and title visible and
// showing a spinner, and restores them when set back to NO.
TEST_F(PromoStyleViewControllerTest, TestContentLoadingState) {
  view_controller_.contentLoading = YES;
  [view_controller_ loadViewIfNeeded];

  EXPECT_FALSE(view_controller_.bannerImageView.hidden);
  EXPECT_FALSE(view_controller_.titleLabel.hidden);
  EXPECT_TRUE(view_controller_.subtitleLabel.hidden);
  EXPECT_TRUE(view_controller_.specificContentView.hidden);
  EXPECT_TRUE(view_controller_.disclaimerView.hidden);
  EXPECT_FALSE(view_controller_.scrollEnabled);
  EXPECT_TRUE(view_controller_.configuration.hideButtons);
  EXPECT_FALSE(view_controller_.hasVisibleButtons);
  EXPECT_TRUE(view_controller_.contentLoadingSpinner.isAnimating);

  view_controller_.contentLoading = NO;

  EXPECT_FALSE(view_controller_.bannerImageView.hidden);
  EXPECT_FALSE(view_controller_.titleLabel.hidden);
  EXPECT_FALSE(view_controller_.subtitleLabel.hidden);
  EXPECT_FALSE(view_controller_.specificContentView.hidden);
  EXPECT_FALSE(view_controller_.disclaimerView.hidden);
  EXPECT_TRUE(view_controller_.scrollEnabled);
  EXPECT_FALSE(view_controller_.configuration.hideButtons);
  EXPECT_TRUE(view_controller_.hasVisibleButtons);
  EXPECT_FALSE(view_controller_.contentLoadingSpinner.isAnimating);
  EXPECT_TRUE(view_controller_.contentLoadingSpinner.hidden);
}

// Tests that `actionButtonsVisibility` updates made while `contentLoading` is
// YES keep the buttons and disclaimer hidden until `contentLoading` becomes NO.
TEST_F(PromoStyleViewControllerTest,
       TestContentLoadingWithActionButtonsVisibility) {
  view_controller_.actionButtonsVisibility = ActionButtonsVisibility::kHidden;
  view_controller_.contentLoading = YES;
  [view_controller_ loadViewIfNeeded];

  // Showing buttons while `contentLoading` is YES must keep them hidden until
  // loading finishes.
  view_controller_.actionButtonsVisibility =
      ActionButtonsVisibility::kEquallyWeightedButtonShown;
  EXPECT_TRUE(view_controller_.disclaimerView.hidden);
  EXPECT_TRUE(view_controller_.configuration.hideButtons);

  view_controller_.contentLoading = NO;
  EXPECT_FALSE(view_controller_.disclaimerView.hidden);
  EXPECT_FALSE(view_controller_.configuration.hideButtons);
  EXPECT_EQ(ChromeButtonStyleTertiary,
            view_controller_.configuration.primaryButtonStyle);

  // Hiding buttons while `contentLoading` is YES must keep them hidden after
  // loading finishes.
  view_controller_.contentLoading = YES;
  view_controller_.actionButtonsVisibility = ActionButtonsVisibility::kHidden;
  view_controller_.contentLoading = NO;
  EXPECT_TRUE(view_controller_.disclaimerView.hidden);
  EXPECT_TRUE(view_controller_.configuration.hideButtons);
}

// Tests that when content overflows the scroll view during `contentLoading`,
// `updateViewsForInitialScrollState` is deferred so that updating
// `primaryActionString` before `contentLoading` becomes NO still displays
// `readMoreString` first and restores the updated primary string once the
// bottom is reached.
TEST_F(PromoStyleViewControllerTest,
       TestContentLoadingDefersReadMoreAndPreservesUpdatedPrimaryButton) {
  view_controller_.scrollToEndMandatory = YES;
  view_controller_.readMoreString = @"Read More";
  view_controller_.contentLoading = YES;
  [view_controller_ loadViewIfNeeded];
  [view_controller_.specificContentView.heightAnchor
      constraintEqualToAnchor:view_controller_.view.heightAnchor
                   multiplier:2.0]
      .active = YES;
  [view_controller_.view layoutIfNeeded];

  // Triggering the initial scroll state check while `contentLoading` is YES
  // should defer it instead of caching the initial `primaryActionString`.
  [view_controller_ updateViewsForInitialScrollState];
  EXPECT_FALSE(view_controller_.didReachBottom);
  EXPECT_NSEQ(@"Primary", view_controller_.configuration.primaryActionString);

  // Updating `primaryActionString` while loading should be preserved as the
  // target string to restore after `"Read More"` is dismissed.
  view_controller_.configuration.primaryActionString = @"Updated Primary";
  view_controller_.contentLoading = NO;

  EXPECT_FALSE(view_controller_.didReachBottom);
  EXPECT_NSEQ(@"Read More", view_controller_.configuration.primaryActionString);

  // Reaching the bottom of the content should restore `"Updated Primary"`.
  [view_controller_ handleDidReachBottomOfContent];
  EXPECT_TRUE(view_controller_.didReachBottom);
  EXPECT_NSEQ(@"Updated Primary",
              view_controller_.configuration.primaryActionString);
}

// Tests that when content fits in the scroll view only while the action buttons
// are hidden during `contentLoading`, `updateViewsForInitialScrollState` is
// deferred until the buttons are shown and the scroll view has its final frame.
// TODO(crbug.com/581795152): Re-enable once the test no longer depends on
// OS-specific layout; it fails on iOS 18.
TEST_F(PromoStyleViewControllerTest,
       DISABLED_TestContentLoadingDefersScrollCheckWhileButtonsAreHidden) {
  view_controller_.scrollToEndMandatory = YES;
  view_controller_.readMoreString = @"Read More";
  view_controller_.contentLoading = YES;
  [view_controller_ loadViewIfNeeded];

  // Two layout passes are needed because `viewDidLayoutSubviews` updates the
  // content view height constraint after the first pass.
  [view_controller_.view layoutIfNeeded];
  [view_controller_.view layoutIfNeeded];
  [view_controller_.contentView.heightAnchor
      constraintEqualToConstant:view_controller_.contentView.frame.size.height]
      .active = YES;

  // Triggering the initial scroll state check while `contentLoading` is YES
  // must not mark `didReachBottom` as YES against the button-less scroll view.
  [view_controller_ updateViewsForInitialScrollState];
  EXPECT_FALSE(view_controller_.didReachBottom);

  // When `contentLoading` becomes NO, unhiding the buttons shrinks the scroll
  // view so the content now overflows and `"Read More"` is shown.
  view_controller_.contentLoading = NO;
  EXPECT_FALSE(view_controller_.didReachBottom);
  EXPECT_NSEQ(@"Read More", view_controller_.configuration.primaryActionString);
}

}  // namespace
