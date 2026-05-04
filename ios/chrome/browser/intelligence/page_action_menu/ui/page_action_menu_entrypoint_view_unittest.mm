// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_entrypoint_view.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/shared/ui/elements/new_feature_badge_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

class PageActionMenuEntrypointViewTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    // Disable animations to make tests synchronous.
    [UIView setAnimationsEnabled:NO];
    view_ = [[PageActionMenuEntrypointView alloc] init];
  }

  void TearDown() override {
    view_ = nil;
    [UIView setAnimationsEnabled:YES];
    PlatformTest::TearDown();
  }

  bool HasBadge() const {
    for (UIView* subview in view_.subviews) {
      if ([subview isKindOfClass:[NewFeatureBadgeView class]]) {
        return true;
      }
    }
    return false;
  }

  PageActionMenuEntrypointView* view_;
};

// Tests that the view initializes with correct default properties.
TEST_F(PageActionMenuEntrypointViewTest, Initialization) {
  EXPECT_NE(view_, nil);
  EXPECT_FALSE(view_.translatesAutoresizingMaskIntoConstraints);
  EXPECT_TRUE(view_.pointerInteractionEnabled);
}

// Tests that the accessibility identifier is set correctly for the default
// state.
TEST_F(PageActionMenuEntrypointViewTest, AccessibilityIdentifier_Default) {
  EXPECT_NSEQ(view_.accessibilityIdentifier,
              kAIHubEntrypointAccessibilityIdentifier);
}

// Tests that the accessibility identifier is set correctly when Direct BWG
// entry point is enabled.
TEST_F(PageActionMenuEntrypointViewTest, AccessibilityIdentifier_DirectBWG) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kPageActionMenu, {{kPageActionMenuDirectEntryPointParam, "true"}});

  // Re-initialize view to pickup the flag change.
  PageActionMenuEntrypointView* bwg_view =
      [[PageActionMenuEntrypointView alloc] init];
  EXPECT_NSEQ(bwg_view.accessibilityIdentifier,
              kGeminiDirectEntryPointAccessibilityIdentifier);
}

// Tests that the accessibility label is set correctly for the default state.
TEST_F(PageActionMenuEntrypointViewTest, AccessibilityLabel_Default) {
  NSString* expected_label = l10n_util::GetNSString(
      IDS_IOS_BWG_PAGE_ACTION_MENU_ENTRY_POINT_ACCESSIBILITY_LABEL);
  EXPECT_NSEQ(view_.accessibilityLabel, expected_label);
}

// Tests that the accessibility label is set correctly when Direct BWG entry
// point is enabled.
TEST_F(PageActionMenuEntrypointViewTest, AccessibilityLabel_DirectBWG) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kPageActionMenu, {{kPageActionMenuDirectEntryPointParam, "true"}});

  PageActionMenuEntrypointView* bwg_view =
      [[PageActionMenuEntrypointView alloc] init];
  NSString* expected_label =
      l10n_util::GetNSString(IDS_IOS_BWG_ASK_GEMINI_ACCESSIBILITY_LABEL);
  EXPECT_NSEQ(bwg_view.accessibilityLabel, expected_label);
}

// Tests that setting the new badge visible property adds/removes the subview
// and updates tint color.
TEST_F(PageActionMenuEntrypointViewTest, NewBadgeVisible) {
  EXPECT_FALSE(view_.newBadgeVisible);
  EXPECT_NSEQ(view_.tintColor, [UIColor colorNamed:kToolbarButtonColor]);

  // Check no NewFeatureBadgeView in subviews initially.
  EXPECT_FALSE(HasBadge());

  // Set visible.
  view_.newBadgeVisible = YES;

  EXPECT_NSEQ(view_.tintColor, [UIColor colorNamed:kBlue600Color]);
  EXPECT_TRUE(HasBadge());

  // Set hidden.
  view_.newBadgeVisible = NO;

  EXPECT_NSEQ(view_.tintColor, [UIColor colorNamed:kToolbarButtonColor]);
  EXPECT_FALSE(HasBadge());
}

// Tests that toggleEntryPointHighlight: updates the view's appearance.
TEST_F(PageActionMenuEntrypointViewTest, ToggleHighlight) {
  EXPECT_NSEQ(view_.tintColor, [UIColor colorNamed:kToolbarButtonColor]);

  [view_ toggleEntryPointHighlight:YES];
  EXPECT_NSEQ(view_.tintColor, [UIColor colorNamed:kSolidWhiteColor]);

  [view_ toggleEntryPointHighlight:NO];
  EXPECT_NSEQ(view_.tintColor, [UIColor colorNamed:kToolbarButtonColor]);
}

}  // namespace
