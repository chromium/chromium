// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_shared_tabs_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/composebox/shared/ui/composebox_ui_constants.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

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

class ComposeboxMenuSharedTabsViewControllerTest : public PlatformTest {
 protected:
  ComposeboxMenuSharedTabsViewControllerTest() {
    view_controller_ =
        [[ComposeboxMenuSharedTabsViewController alloc] initWithSharedTabs:@[]];
    // Live Dynamic Type updates are only delivered to views in a window, so
    // host the view controller in a key window.
    scoped_key_window_.Get().rootViewController = view_controller_;
  }

  // Sets the content size category of the window hosting the view controller
  // and lays it out, as if the user changed the text size.
  void SetContentSizeCategory(UIContentSizeCategory category) {
    UIWindow* window = scoped_key_window_.Get();
    window.traitOverrides.preferredContentSizeCategory = category;
    [window layoutIfNeeded];
  }

  // Returns the disclaimer text view.
  UITextView* DisclaimerTextView() {
    UIView* view = FindViewWithAccessibilityIdentifier(
        view_controller_.view,
        kComposeboxSharedTabsDisclaimerAccessibilityIdentifier);
    return base::apple::ObjCCast<UITextView>(view);
  }

  // Returns the font of the disclaimer's attributed text at `index`.
  UIFont* DisclaimerFontAtIndex(NSUInteger index) {
    return [DisclaimerTextView().attributedText attribute:NSFontAttributeName
                                                  atIndex:index
                                           effectiveRange:nullptr];
  }

  ScopedKeyWindow scoped_key_window_;
  ComposeboxMenuSharedTabsViewController* view_controller_;
};

// Test that the disclaimer font grows when the text size changes while the
// Shared Tabs sheet is on screen.
TEST_F(ComposeboxMenuSharedTabsViewControllerTest,
       DisclaimerScalesOnLiveContentSizeChange) {
  SetContentSizeCategory(UIContentSizeCategoryLarge);
  ASSERT_TRUE(DisclaimerTextView());
  ASSERT_GT(DisclaimerTextView().attributedText.length, 0u);
  UIFont* large_font = DisclaimerFontAtIndex(0);
  ASSERT_TRUE(large_font);

  SetContentSizeCategory(
      UIContentSizeCategoryAccessibilityExtraExtraExtraLarge);
  EXPECT_GT(DisclaimerFontAtIndex(0).pointSize, large_font.pointSize);
}

// Test that the "Learn more" link survives a text size change.
TEST_F(ComposeboxMenuSharedTabsViewControllerTest,
       DisclaimerKeepsLinkOnLiveContentSizeChange) {
  SetContentSizeCategory(UIContentSizeCategoryLarge);
  ASSERT_TRUE(DisclaimerTextView());
  NSAttributedString* text = DisclaimerTextView().attributedText;
  ASSERT_GT(text.length, 0u);
  // The link is at the end of the disclaimer.
  id large_link = [text attribute:NSLinkAttributeName
                          atIndex:text.length - 1
                   effectiveRange:nullptr];
  ASSERT_TRUE(large_link);

  SetContentSizeCategory(
      UIContentSizeCategoryAccessibilityExtraExtraExtraLarge);
  text = DisclaimerTextView().attributedText;
  ASSERT_GT(text.length, 0u);
  EXPECT_NSEQ(large_link, [text attribute:NSLinkAttributeName
                                     atIndex:text.length - 1
                              effectiveRange:nullptr]);
}
