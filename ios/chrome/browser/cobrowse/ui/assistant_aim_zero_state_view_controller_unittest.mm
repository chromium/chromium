// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_zero_state_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_ui_constants.h"
#import "ios/chrome/test/scoped_key_window.h"
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

}  // namespace

class AssistantAIMZeroStateViewControllerTest : public PlatformTest {
 protected:
  AssistantAIMZeroStateViewControllerTest() {
    view_controller_ = [[AssistantAIMZeroStateViewController alloc] init];
    // Live Dynamic Type updates are only delivered to views in a window, so
    // host the view controller in a key window.
    scoped_key_window_.Get().rootViewController = view_controller_;
    // Use the localized production greeting so the test shows the same text
    // as the app, whatever the test locale.
    greeting_message_ = l10n_util::GetNSString(
        IDS_AI_MODE_FRIENDLY_ZERO_STATE_TITLE_WITHOUT_NAME);
    view_controller_.greetingMessage = greeting_message_;
  }

  // Sets the content size category of the window hosting the view controller
  // and lays it out, as if the user changed the text size.
  void SetContentSizeCategory(UIContentSizeCategory category) {
    UIWindow* window = scoped_key_window_.Get();
    window.traitOverrides.preferredContentSizeCategory = category;
    [window layoutIfNeeded];
  }

  // Returns the greeting label.
  UILabel* GreetingLabel() {
    UIView* view = FindViewWithAccessibilityIdentifier(
        view_controller_.view,
        kAssistantAIMZeroStateGreetingAccessibilityIdentifier);
    return base::apple::ObjCCast<UILabel>(view);
  }

  ScopedKeyWindow scoped_key_window_;
  AssistantAIMZeroStateViewController* view_controller_;
  NSString* greeting_message_;
};

// Test that the greeting font grows when the text size changes while the zero
// state is on screen.
TEST_F(AssistantAIMZeroStateViewControllerTest,
       GreetingScalesOnLiveContentSizeChange) {
  SetContentSizeCategory(UIContentSizeCategoryLarge);
  UILabel* label = GreetingLabel();
  ASSERT_TRUE(label);
  EXPECT_NSEQ(greeting_message_, label.text);
  CGFloat large_point_size = label.font.pointSize;

  SetContentSizeCategory(
      UIContentSizeCategoryAccessibilityExtraExtraExtraLarge);
  EXPECT_GT(GreetingLabel().font.pointSize, large_point_size);
}
