// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view.h"

#import <UIKit/UIKit.h>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/browser/ntp/ui_bundled/fake_location_bar_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@interface NewTabPageHeaderView (Testing)
@property(nonatomic, readonly) UIButton* voiceSearchButton;
@property(nonatomic, readonly) UIButton* plusButton;
@end

namespace {

constexpr CGFloat kHeaderWidth = 800.0;
constexpr CGFloat kHeaderHeight = 300.0;
constexpr CGFloat kPinnedScrollOffset = 9000.0;
constexpr CGFloat kExpectedIconContainerSize = 24.0;
constexpr CGFloat kExpectedLogoSize = 22.0;
constexpr CGFloat kExpectedFakeboxHorizontalMargin = 20.0;
constexpr CGFloat kExpectedFakeboxVerticalMargin = 24.0;
constexpr CGFloat kExpectedButtonTopContentInset = 2.0;
constexpr CGFloat kExpectedLogoCenterX = 32.0;
constexpr CGFloat kExpectedLogoCenterY = 36.0;
constexpr CGFloat kExpectedOmniboxContainerLeadingMargin = 21.0;
constexpr CGFloat kExpectedOmniboxLogoLeadingMargin = 22.0;

class NewTabPageHeaderViewTest : public PlatformTest {
 protected:
  // Creates, configures, and lays out a `NewTabPageHeaderView` at scroll
  // offset 0.
  NewTabPageHeaderView* CreateHeaderView() {
    NewTabPageHeaderView* header_view =
        [[NewTabPageHeaderView alloc] initWithUseNewBadgeForLensButton:NO
                                       useNewBadgeForCustomizationMenu:NO];
    command_handler_ = OCMProtocolMock(@protocol(NewTabPageHeaderCommands));
    header_view.commandHandler = command_handler_;
    [header_view setupSubviews];
    header_view.isGoogleDefaultSearchEngine = YES;
    search_engine_logo_view_ = [[UIView alloc] init];
    header_view.searchEngineLogoView = search_engine_logo_view_;
    [header_view searchEngineLogoStateDidChange:SearchEngineLogoState::kLogo];

    header_view.translatesAutoresizingMaskIntoConstraints = YES;
    header_view.frame = CGRectMake(0, 0, kHeaderWidth, kHeaderHeight);
    [header_view updateFakeOmniboxForOffset:0
                                screenWidth:kHeaderWidth
                             safeAreaInsets:UIEdgeInsetsZero
                     animateScrollAnimation:YES];
    [header_view layoutIfNeeded];
    return header_view;
  }

 private:
  id<NewTabPageHeaderCommands> command_handler_;
  UIView* search_engine_logo_view_;
};

// Tests that when `kNewTabPageUICleanup` is enabled, the fakebox leading logo
// view (22x22) is centered inside a non-interactive 24x24 container at 20pt
// from the leading edge (and 21pt when scrolled to the pinned omnibox so the
// 22pt logo stays at 22pt), and the trailing voice search button is sized to
// 24x24 with 20pt trailing margin, 24pt vertical margins, and a 2pt top content
// inset.
TEST_F(NewTabPageHeaderViewTest, FakeboxIconSizingUICleanup) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kNewTabPageUICleanup},
                                {kChromeNextIa, kPlusButtonInFakebox});

  NewTabPageHeaderView* header_view = CreateHeaderView();

  UIView* fakebox = [header_view fakeOmniboxView];
  ASSERT_NE(nil, fakebox);

  ASSERT_NE(nil, header_view.voiceSearchButton);
  CGRect voice_frame_in_fakebox =
      [fakebox convertRect:header_view.voiceSearchButton.bounds
                  fromView:header_view.voiceSearchButton];
  EXPECT_FLOAT_EQ(kExpectedIconContainerSize,
                  voice_frame_in_fakebox.size.width);
  EXPECT_FLOAT_EQ(kExpectedIconContainerSize,
                  voice_frame_in_fakebox.size.height);
  EXPECT_FLOAT_EQ(kExpectedFakeboxVerticalMargin,
                  voice_frame_in_fakebox.origin.y);
  EXPECT_FLOAT_EQ(fakebox.bounds.size.width - kExpectedFakeboxHorizontalMargin,
                  CGRectGetMaxX(voice_frame_in_fakebox));
  EXPECT_FLOAT_EQ(
      kExpectedButtonTopContentInset,
      header_view.voiceSearchButton.configuration.contentInsets.top);

  UIImageView* logo_image_view =
      chrome_test_util::FindViewWithPredicate<UIImageView>(
          fakebox, ^BOOL(UIView* view) {
            return [view isKindOfClass:[UIImageView class]] &&
                   view.superview.superview == fakebox &&
                   [view.superview isMemberOfClass:[UIView class]];
          });
  ASSERT_NE(nil, logo_image_view);
  EXPECT_FLOAT_EQ(kExpectedLogoSize, logo_image_view.bounds.size.width);
  EXPECT_FLOAT_EQ(kExpectedLogoSize, logo_image_view.bounds.size.height);

  UIView* logo_container = logo_image_view.superview;
  EXPECT_FLOAT_EQ(kExpectedIconContainerSize, logo_container.frame.size.width);
  EXPECT_FLOAT_EQ(kExpectedIconContainerSize, logo_container.frame.size.height);
  EXPECT_FLOAT_EQ(kExpectedFakeboxHorizontalMargin,
                  logo_container.frame.origin.x);
  EXPECT_FLOAT_EQ(kExpectedFakeboxVerticalMargin,
                  logo_container.frame.origin.y);
  CGPoint logo_center_in_fakebox =
      [fakebox convertPoint:logo_image_view.center
                   fromView:logo_image_view.superview];
  EXPECT_FLOAT_EQ(kExpectedLogoCenterX, logo_center_in_fakebox.x);
  EXPECT_FLOAT_EQ(kExpectedLogoCenterY, logo_center_in_fakebox.y);

  // Taps on the 24x24 logo container should pass through to `fakeLocationBar`.
  FakeLocationBarView* fake_location_bar =
      chrome_test_util::FindViewByClass<FakeLocationBarView>(fakebox);
  ASSERT_NE(nil, fake_location_bar);
  EXPECT_EQ(fake_location_bar, [fakebox hitTest:logo_center_in_fakebox
                                      withEvent:nil]);

  // Verify that when scrolled to the pinned omnibox position, the 24x24 logo
  // container is positioned at 21pt so the 22x22 logo sits at 22pt.
  [header_view updateFakeOmniboxForOffset:kPinnedScrollOffset
                              screenWidth:kHeaderWidth
                           safeAreaInsets:UIEdgeInsetsZero
                   animateScrollAnimation:YES];
  [fakebox setNeedsLayout];
  [fakebox layoutIfNeeded];
  EXPECT_FLOAT_EQ(kExpectedOmniboxContainerLeadingMargin,
                  logo_container.frame.origin.x);
  CGRect logo_frame_in_fakebox = [fakebox convertRect:logo_image_view.bounds
                                             fromView:logo_image_view];
  EXPECT_FLOAT_EQ(kExpectedOmniboxLogoLeadingMargin,
                  logo_frame_in_fakebox.origin.x);
}

// Tests that when `kNewTabPageUICleanup` and `kPlusButtonInFakebox` are
// enabled, the plus button is sized to 24x24 with 20pt leading margin and 24pt
// vertical margins.
TEST_F(NewTabPageHeaderViewTest, FakeboxPlusButtonSizingUICleanup) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({kNewTabPageUICleanup, kPlusButtonInFakebox},
                                {kChromeNextIa});

  NewTabPageHeaderView* header_view = CreateHeaderView();
  [header_view setAIMAllowed:YES];
  [header_view setFuseboxEligible:YES];
  [header_view layoutIfNeeded];

  ASSERT_NE(nil, header_view.plusButton);
  EXPECT_FLOAT_EQ(kExpectedIconContainerSize,
                  header_view.plusButton.frame.size.width);
  EXPECT_FLOAT_EQ(kExpectedIconContainerSize,
                  header_view.plusButton.frame.size.height);
  EXPECT_FLOAT_EQ(kExpectedFakeboxHorizontalMargin,
                  header_view.plusButton.frame.origin.x);
  EXPECT_FLOAT_EQ(kExpectedFakeboxVerticalMargin,
                  header_view.plusButton.frame.origin.y);
}

}  // namespace
