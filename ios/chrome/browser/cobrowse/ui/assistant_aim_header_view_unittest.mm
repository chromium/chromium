// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_header_view.h"

#import <UIKit/UIKit.h>

#import <string>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_ui_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

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

// Appends every `UIButton` in the hierarchy of `view` to `buttons`.
void CollectButtons(UIView* view, NSMutableArray<UIButton*>* buttons) {
  if ([view isKindOfClass:[UIButton class]]) {
    [buttons addObject:base::apple::ObjCCastStrict<UIButton>(view)];
  }
  for (UIView* subview in view.subviews) {
    CollectButtons(subview, buttons);
  }
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

// Test that each header button has the expected accessibility identifier and
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
    UIButton* button = base::apple::ObjCCast<UIButton>(
        FindViewWithAccessibilityIdentifier(header_view_, expected.identifier));
    ASSERT_TRUE(button);
    ASSERT_GT(expected.label.length, 0u);
    EXPECT_NSEQ(expected.label, button.accessibilityLabel);
  }
}

// Test that every button in the header, including the ones hidden in the
// current mode, has an accessibility label and identifier.
TEST_F(AssistantAIMHeaderViewTest, AllButtonsAreLabeledAndIdentified) {
  NSMutableArray<UIButton*>* buttons = [NSMutableArray array];
  CollectButtons(header_view_, buttons);
  ASSERT_GT(buttons.count, 0u);

  for (UIButton* button in buttons) {
    SCOPED_TRACE(DescribeButton(button));
    EXPECT_GT(button.accessibilityLabel.length, 0u);
    EXPECT_GT(button.accessibilityIdentifier.length, 0u);
  }
}
