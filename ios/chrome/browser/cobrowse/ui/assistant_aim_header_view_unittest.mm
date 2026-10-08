// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_header_view.h"

#import <UIKit/UIKit.h>

#import <string>

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_ui_constants.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Returns whether `view` has a `UILargeContentViewerInteraction`.
bool HasLargeContentViewerInteraction(UIView* view) {
  for (id<UIInteraction> interaction in view.interactions) {
    if ([interaction isKindOfClass:[UILargeContentViewerInteraction class]]) {
      return true;
    }
  }
  return false;
}

// Returns a description of `button`'s accessibility label and identifier, used
// to trace failures.
std::string DescribeButton(UIButton* button) {
  return "label: \"" + base::SysNSStringToUTF8(button.accessibilityLabel) +
         "\", identifier: \"" +
         base::SysNSStringToUTF8(button.accessibilityIdentifier) + "\"";
}

}  // namespace

class AssistantAIMHeaderViewTest : public PlatformTest {
 protected:
  AssistantAIMHeaderViewTest() {
    header_view_ = [[AssistantAIMHeaderView alloc] init];
  }

  AssistantAIMHeaderView* header_view_;
};

// Tests that each header button has the expected accessibility identifier and
// localized accessibility label, so VoiceOver users know what it does.
TEST_F(AssistantAIMHeaderViewTest, ButtonsHaveAccessibilityLabels) {
  struct ExpectedButton {
    NSString* identifier;
    NSString* label;
  };
  const ExpectedButton expected_buttons[] = {
      {kAssistantAIMBackButtonAccessibilityIdentifier,
       l10n_util::GetNSString(IDS_IOS_ICON_ARROW_BACK)},
      {kAssistantAIMCloseButtonAccessibilityIdentifier,
       l10n_util::GetNSString(IDS_IOS_ICON_CLOSE)},
      {kAssistantAIMContextMenuButtonAccessibilityIdentifier,
       l10n_util::GetNSString(
           IDS_CONTEXTUAL_TASKS_SIDE_PANEL_MORE_OPTIONS_TOOL_TIP)},
      {kAssistantAIMHistoryButtonAccessibilityIdentifier,
       l10n_util::GetNSString(
           IDS_CONTEXTUAL_TASKS_SIDE_PANEL_HISTORY_TOOL_TIP)},
      {kAssistantAIMNewThreadButtonAccessibilityIdentifier,
       l10n_util::GetNSString(
           IDS_CONTEXTUAL_TASKS_SIDE_PANEL_NEW_THREAD_TOOL_TIP)},
  };

  for (const ExpectedButton& expected : expected_buttons) {
    SCOPED_TRACE(base::SysNSStringToUTF8(expected.identifier));
    UIButton* button = chrome_test_util::FindViewById<UIButton>(
        header_view_, expected.identifier);
    ASSERT_TRUE(button);
    ASSERT_GT(expected.label.length, 0u);
    EXPECT_NSEQ(expected.label, button.accessibilityLabel);
  }
}

// Tests that every button in the header, including the ones hidden in the
// current mode, has an accessibility label and identifier.
TEST_F(AssistantAIMHeaderViewTest, AllButtonsAreLabeledAndIdentified) {
  NSArray<UIButton*>* buttons =
      chrome_test_util::FindViewsByClass<UIButton>(header_view_);
  ASSERT_GT(buttons.count, 0u);

  for (UIButton* button in buttons) {
    SCOPED_TRACE(DescribeButton(button));
    EXPECT_GT(button.accessibilityLabel.length, 0u);
    EXPECT_GT(button.accessibilityIdentifier.length, 0u);
  }
}

// Tests that the visible header action buttons in the pill fill the 40pt
// capsule height and receive touches 1pt inside the top and bottom edges of the
// pill.
TEST_F(AssistantAIMHeaderViewTest, ActionButtonsFillPillHeight) {
  constexpr CGFloat kHeaderWidth = 400.0;
  constexpr CGFloat kHeaderHeight = 40.0;
  constexpr CGFloat kEdgeInset = 1.0;

  header_view_.frame = CGRectMake(0, 0, kHeaderWidth, kHeaderHeight);

  struct ModeTestCase {
    AssistantAIMState mode;
    const char* mode_name;
    NSArray<NSString*>* visible_identifiers;
  };
  const ModeTestCase test_cases[] = {
      {AssistantAIMState::kThread, "Thread",
       @[
         kAssistantAIMNewThreadButtonAccessibilityIdentifier,
         kAssistantAIMHistoryButtonAccessibilityIdentifier,
       ]},
      {AssistantAIMState::kHistory, "History",
       @[
         kAssistantAIMNewThreadButtonAccessibilityIdentifier,
         kAssistantAIMContextMenuButtonAccessibilityIdentifier,
       ]},
  };

  for (const ModeTestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.mode_name);
    [header_view_ setMode:test_case.mode];
    [header_view_ setNeedsLayout];
    [header_view_ layoutIfNeeded];

    for (NSString* identifier in test_case.visible_identifiers) {
      SCOPED_TRACE(base::SysNSStringToUTF8(identifier));
      UIButton* button =
          chrome_test_util::FindViewById<UIButton>(header_view_, identifier);
      ASSERT_TRUE(button);
      ASSERT_FALSE(button.hidden);
      ASSERT_TRUE(button.superview);

      EXPECT_EQ(kHeaderHeight, CGRectGetHeight(button.frame));

      CGRect pill_frame = [button.superview convertRect:button.superview.bounds
                                                 toView:header_view_];
      CGRect button_frame = [button convertRect:button.bounds
                                         toView:header_view_];
      CGFloat center_x = CGRectGetMidX(button_frame);

      CGPoint top_point =
          CGPointMake(center_x, CGRectGetMinY(pill_frame) + kEdgeInset);
      CGPoint bottom_point =
          CGPointMake(center_x, CGRectGetMaxY(pill_frame) - kEdgeInset);

      EXPECT_NSEQ(button, [header_view_ hitTest:top_point withEvent:nil]);
      EXPECT_NSEQ(button, [header_view_ hitTest:bottom_point withEvent:nil]);
    }
  }
}

// Tests that both the back and close buttons use `ExtendedTouchTargetButton`
// (accepting touches within a 44pt diameter circle outside their 40x40pt
// bounds) and set `tintColor` to `clearColor` to avoid a tinted glass rim.
TEST_F(AssistantAIMHeaderViewTest,
       BackAndCloseButtonsHaveClearTintAndExtendedTouchTarget) {
  constexpr CGFloat kHeaderWidth = 400.0;
  constexpr CGFloat kHeaderHeight = 40.0;
  constexpr CGFloat kButtonCenter = 20.0;
  constexpr CGFloat kExtendedPointOffset = -1.0;

  header_view_.frame = CGRectMake(0, 0, kHeaderWidth, kHeaderHeight);
  [header_view_ setMode:AssistantAIMState::kHistory];
  [header_view_ setNeedsLayout];
  [header_view_ layoutIfNeeded];

  NSArray<NSString*>* identifiers = @[
    kAssistantAIMBackButtonAccessibilityIdentifier,
    kAssistantAIMCloseButtonAccessibilityIdentifier,
  ];

  for (NSString* identifier in identifiers) {
    SCOPED_TRACE(base::SysNSStringToUTF8(identifier));
    ExtendedTouchTargetButton* button =
        chrome_test_util::FindViewById<ExtendedTouchTargetButton>(header_view_,
                                                                  identifier);
    ASSERT_TRUE(button);
    EXPECT_NSEQ([UIColor clearColor], button.tintColor);
    EXPECT_TRUE([button
        pointInside:CGPointMake(kButtonCenter, kExtendedPointOffset)
          withEvent:nil]);
  }
}

// Tests that the header title label caps Dynamic Type scaling at
// `UIContentSizeCategoryExtraExtraExtraLarge` so it fits within the 40pt header
// bar, and enables `UILargeContentViewer` on the header view.
TEST_F(AssistantAIMHeaderViewTest,
       TitleLabelCapsDynamicTypeAndShowsLargeContentViewer) {
  constexpr CGFloat kHeaderWidth = 400.0;
  constexpr CGFloat kHeaderHeight = 40.0;

  ScopedKeyWindow scoped_key_window;
  UIWindow* window = scoped_key_window.Get();
  header_view_.frame = CGRectMake(0, 0, kHeaderWidth, kHeaderHeight);
  [window addSubview:header_view_];
  [header_view_ setMode:AssistantAIMState::kHistory];

  UILabel* title_label = chrome_test_util::FindViewById<UILabel>(
      header_view_, kAssistantAIMTitleLabelAccessibilityIdentifier);
  ASSERT_TRUE(title_label);
  EXPECT_TRUE(title_label.adjustsFontForContentSizeCategory);
  EXPECT_NSEQ(UIContentSizeCategoryExtraExtraExtraLarge,
              title_label.maximumContentSizeCategory);
  EXPECT_TRUE(title_label.showsLargeContentViewer);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_AIM_HISTORY), title_label.text);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_AIM_HISTORY),
              title_label.largeContentTitle);
  EXPECT_TRUE(HasLargeContentViewerInteraction(header_view_));

  window.traitOverrides.preferredContentSizeCategory =
      UIContentSizeCategoryLarge;
  [window layoutIfNeeded];
  [header_view_ layoutIfNeeded];
  CGFloat large_point_size = title_label.font.pointSize;

  window.traitOverrides.preferredContentSizeCategory =
      UIContentSizeCategoryExtraExtraExtraLarge;
  [window layoutIfNeeded];
  [header_view_ layoutIfNeeded];
  CGFloat xxxl_point_size = title_label.font.pointSize;
  EXPECT_GT(xxxl_point_size, large_point_size);

  window.traitOverrides.preferredContentSizeCategory =
      UIContentSizeCategoryAccessibilityExtraExtraExtraLarge;
  [window layoutIfNeeded];
  [header_view_ layoutIfNeeded];
  EXPECT_EQ(xxxl_point_size, title_label.font.pointSize);
  EXPECT_LE(CGRectGetHeight(title_label.frame), kHeaderHeight);
}

// Tests that all five header icon buttons enable `UILargeContentViewer` with a
// scaled symbol image and localized title, and that the header view registers a
// `UILargeContentViewerInteraction`.
TEST_F(AssistantAIMHeaderViewTest, ButtonsShowLargeContentViewer) {
  EXPECT_TRUE(HasLargeContentViewerInteraction(header_view_));

  struct ExpectedButton {
    NSString* identifier;
    NSString* title;
  };
  const ExpectedButton expected_buttons[] = {
      {kAssistantAIMBackButtonAccessibilityIdentifier,
       l10n_util::GetNSString(IDS_IOS_ICON_ARROW_BACK)},
      {kAssistantAIMCloseButtonAccessibilityIdentifier,
       l10n_util::GetNSString(IDS_IOS_ICON_CLOSE)},
      {kAssistantAIMContextMenuButtonAccessibilityIdentifier,
       l10n_util::GetNSString(
           IDS_CONTEXTUAL_TASKS_SIDE_PANEL_MORE_OPTIONS_TOOL_TIP)},
      {kAssistantAIMHistoryButtonAccessibilityIdentifier,
       l10n_util::GetNSString(
           IDS_CONTEXTUAL_TASKS_SIDE_PANEL_HISTORY_TOOL_TIP)},
      {kAssistantAIMNewThreadButtonAccessibilityIdentifier,
       l10n_util::GetNSString(
           IDS_CONTEXTUAL_TASKS_SIDE_PANEL_NEW_THREAD_TOOL_TIP)},
  };

  for (const ExpectedButton& expected : expected_buttons) {
    SCOPED_TRACE(base::SysNSStringToUTF8(expected.identifier));
    UIButton* button = chrome_test_util::FindViewById<UIButton>(
        header_view_, expected.identifier);
    ASSERT_TRUE(button);
    EXPECT_TRUE(button.showsLargeContentViewer);
    EXPECT_TRUE(button.scalesLargeContentImage);
    EXPECT_NSEQ(expected.title, button.largeContentTitle);
    ASSERT_TRUE(button.largeContentImage);
    EXPECT_TRUE(button.largeContentImage.isSymbolImage);
  }
}
