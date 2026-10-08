// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_history_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_ui_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Dimensions of the history container in the Medium detent on a compact iPhone
// (375x667pt iPhone SE 3rd generation).
constexpr CGFloat kShortContainerWidth = 375.0;
constexpr CGFloat kShortContainerHeight = 229.0;

// Tolerance for vertical centering checks.
constexpr CGFloat kCenteringTolerance = 1.0;

// Returns the first subview of `view` (or `view` itself) whose accessibility
// identifier is `identifier`, or nil if there is none.
UIView* FindViewWithAccessibilityIdentifier(UIView* view,
                                            NSString* identifier) {
  if ([view.accessibilityIdentifier isEqualToString:identifier]) {
    return view;
  }
  for (UIView* subview in view.subviews) {
    UIView* match = FindViewWithAccessibilityIdentifier(subview, identifier);
    if (match) {
      return match;
    }
  }
  return nil;
}

// Returns the nearest enclosing `UIScrollView` ancestor of `view`, or nil if
// there is none.
UIScrollView* FindScrollViewAncestor(UIView* view) {
  for (UIView* ancestor = view.superview; ancestor != nil;
       ancestor = ancestor.superview) {
    if ([ancestor isKindOfClass:[UIScrollView class]]) {
      return base::apple::ObjCCastStrict<UIScrollView>(ancestor);
    }
  }
  return nil;
}

}  // namespace

class AssistantAIMHistoryViewControllerTest : public PlatformTest {
 protected:
  AssistantAIMHistoryViewControllerTest() {
    host_view_controller_ = [[UIViewController alloc] init];
    scoped_key_window_.Get().rootViewController = host_view_controller_;

    view_controller_ = [[AssistantAIMHistoryViewController alloc] init];
    [host_view_controller_ addChildViewController:view_controller_];
    view_controller_.view.translatesAutoresizingMaskIntoConstraints = NO;
    [host_view_controller_.view addSubview:view_controller_.view];
    [NSLayoutConstraint activateConstraints:@[
      [view_controller_.view.topAnchor
          constraintEqualToAnchor:host_view_controller_.view.safeAreaLayoutGuide
                                      .topAnchor],
      [view_controller_.view.leadingAnchor
          constraintEqualToAnchor:host_view_controller_.view.leadingAnchor],
      [view_controller_.view.widthAnchor
          constraintEqualToConstant:kShortContainerWidth],
      [view_controller_.view.heightAnchor
          constraintEqualToConstant:kShortContainerHeight],
    ]];
    [view_controller_ didMoveToParentViewController:host_view_controller_];
  }

  // Sets the content size category of the window hosting the view controller
  // and lays out the hierarchy, as if the user changed the text size.
  void SetContentSizeCategory(UIContentSizeCategory category) {
    UIWindow* window = scoped_key_window_.Get();
    window.traitOverrides.preferredContentSizeCategory = category;
    [window setNeedsLayout];
    [window layoutIfNeeded];
    [view_controller_.view setNeedsLayout];
    [view_controller_.view layoutIfNeeded];
  }

  // Returns the signed-out zero-state stack view.
  UIStackView* SignedOutStackView() {
    UIView* view = FindViewWithAccessibilityIdentifier(
        view_controller_.view,
        kAssistantAIMHistorySignedOutViewAccessibilityIdentifier);
    return base::apple::ObjCCast<UIStackView>(view);
  }

  ScopedKeyWindow scoped_key_window_;
  UIViewController* host_view_controller_;
  AssistantAIMHistoryViewController* view_controller_;
};

// Tests that the signed-out zero state stays vertically centered when it fits
// at standard text sizes (`UIContentSizeCategoryLarge`) and becomes vertically
// scrollable without overflowing above the container when it exceeds the Medium
// detent height at `UIContentSizeCategoryAccessibilityExtraExtraExtraLarge`.
TEST_F(AssistantAIMHistoryViewControllerTest,
       SignedOutZeroStateCentersWhenFitsAndScrollsAtAX5) {
  SetContentSizeCategory(UIContentSizeCategoryLarge);

  EXPECT_FALSE(view_controller_.signedIn);
  EXPECT_TRUE(view_controller_.view.clipsToBounds);

  UIStackView* stack_view = SignedOutStackView();
  ASSERT_TRUE(stack_view);
  ASSERT_EQ(2u, stack_view.arrangedSubviews.count);

  UILabel* title_label =
      base::apple::ObjCCast<UILabel>(stack_view.arrangedSubviews[0]);
  UILabel* subtitle_label =
      base::apple::ObjCCast<UILabel>(stack_view.arrangedSubviews[1]);
  ASSERT_TRUE(title_label);
  ASSERT_TRUE(subtitle_label);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_AIM_HISTORY_SIGNED_OUT_TITLE),
              title_label.text);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_AIM_HISTORY_SIGNED_OUT_SUBTITLE),
              subtitle_label.text);

  UIScrollView* scroll_view = FindScrollViewAncestor(stack_view);
  ASSERT_TRUE(scroll_view);
  EXPECT_TRUE(scroll_view.clipsToBounds);
  EXPECT_FALSE(scroll_view.hidden);
  EXPECT_EQ(kShortContainerHeight, CGRectGetHeight(scroll_view.bounds));

  CGRect large_stack_frame_in_scroll = [stack_view convertRect:stack_view.bounds
                                                        toView:scroll_view];
  EXPECT_GE(CGRectGetMinY(large_stack_frame_in_scroll), 0.0);
  EXPECT_LE(CGRectGetMaxY(large_stack_frame_in_scroll), kShortContainerHeight);
  EXPECT_EQ(kShortContainerHeight, scroll_view.contentSize.height);
  EXPECT_NEAR(CGRectGetMidY(scroll_view.bounds),
              CGRectGetMidY(large_stack_frame_in_scroll), kCenteringTolerance);

  CGFloat large_title_point_size = title_label.font.pointSize;
  CGFloat large_subtitle_point_size = subtitle_label.font.pointSize;

  SetContentSizeCategory(
      UIContentSizeCategoryAccessibilityExtraExtraExtraLarge);

  EXPECT_GT(title_label.font.pointSize, large_title_point_size);
  EXPECT_GT(subtitle_label.font.pointSize, large_subtitle_point_size);

  CGRect ax5_stack_frame_in_scroll = [stack_view convertRect:stack_view.bounds
                                                      toView:scroll_view];
  CGRect ax5_stack_frame_in_root =
      [stack_view convertRect:stack_view.bounds toView:view_controller_.view];

  EXPECT_GE(CGRectGetMinY(ax5_stack_frame_in_scroll), 0.0);
  EXPECT_GE(CGRectGetMinY(ax5_stack_frame_in_root), 0.0);
  EXPECT_GT(CGRectGetHeight(ax5_stack_frame_in_scroll), kShortContainerHeight);
  EXPECT_GT(scroll_view.contentSize.height,
            CGRectGetHeight(scroll_view.bounds));
  EXPECT_GE(scroll_view.contentSize.height,
            CGRectGetMaxY(ax5_stack_frame_in_scroll));
}

// Tests that toggling `signedIn` hides and shows the signed-out zero state
// scroll view.
TEST_F(AssistantAIMHistoryViewControllerTest,
       SignedInTogglesSignedOutZeroStateVisibility) {
  SetContentSizeCategory(UIContentSizeCategoryLarge);

  UIStackView* stack_view = SignedOutStackView();
  ASSERT_TRUE(stack_view);
  UIScrollView* scroll_view = FindScrollViewAncestor(stack_view);
  ASSERT_TRUE(scroll_view);
  EXPECT_FALSE(scroll_view.hidden);

  view_controller_.signedIn = YES;
  EXPECT_TRUE(scroll_view.hidden);

  view_controller_.signedIn = NO;
  EXPECT_FALSE(scroll_view.hidden);
}
