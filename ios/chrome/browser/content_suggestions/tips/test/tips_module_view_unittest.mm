// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tips/ui/tips_module_view.h"

#import <Foundation/Foundation.h>

#import "base/test/task_environment.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "ios/chrome/browser/content_suggestions/tips/ui/tips_module_config.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_view.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/platform_test.h"

using chrome_test_util::ExpectUniqueSubview;
using segmentation_platform::TipIdentifier;

// Tests the `TipsModuleView` and subviews.
class TipsModuleViewTest : public PlatformTest {
 public:
  TipsModuleViewTest() {
    _superview = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)];

    _window = [[UIWindow alloc]
        initWithWindowScene:chrome_test_util::GetAnyWindowScene()];

    [_window addSubview:_superview];

    UIView.animationsEnabled = NO;
  }

  // Expects a subview with the given `accessibility_id` to either exist or
  // or not.
  void ExpectSubview(NSString* accessibility_id, bool exists) {
    UIView* subview =
        chrome_test_util::FindViewById(_superview, accessibility_id);

    if (exists) {
      EXPECT_NE(subview, nil);
    } else {
      EXPECT_EQ(subview, nil);
    }
  }

 protected:
  base::test::SingleThreadTaskEnvironment _task_environment;
  UIWindow* _window;
  UIView* _superview;
};

// Tests that the module can be initialized, create subviews, and that the
// correct module state is displayed.
TEST_F(TipsModuleViewTest, DisplaysModuleWithDefaultState) {
  TipsModuleConfig* config = [[TipsModuleConfig alloc]
      initWithTipIdentifier:TipIdentifier::kLensTranslate];

  TipsModuleView* view = [[TipsModuleView alloc] initWithConfig:config];

  [_superview addSubview:view];

  // It should initially display one item, i.e. the hero-cell default layout
  // item.
  ExpectUniqueSubview<IconDetailView>(_superview);

  ExpectSubview(@"kTipsModuleViewID", true);
  ExpectSubview(@"kLensTranslateAccessibilityID", true);
  ExpectSubview(@"kLensShopAccessibilityID", false);
}

// Tests that the module displays the correct accessibility identifier for
// each tip type.
TEST_F(TipsModuleViewTest, DisplaysCorrectAccessibilityIdentifier) {
  std::vector<TipIdentifier> tips = {
      TipIdentifier::kLensSearch,           TipIdentifier::kLensShop,
      TipIdentifier::kLensTranslate,        TipIdentifier::kAddressBarPosition,
      TipIdentifier::kSavePasswords,        TipIdentifier::kAutofillPasswords,
      TipIdentifier::kEnhancedSafeBrowsing,
  };

  std::vector<NSString*> identifiers = {
      @"kLensSearchAccessibilityID",
      @"kLensShopAccessibilityID",
      @"kLensTranslateAccessibilityID",
      @"kAddressBarPositionAccessibilityID",
      @"kSavePasswordsAccessibilityID",
      @"kAutofillPasswordsAccessibilityID",
      @"kEnhancedSafeBrowsingAccessibilityID",
  };

  ASSERT_EQ(tips.size(), identifiers.size());

  for (size_t i = 0; i < tips.size(); ++i) {
    TipsModuleConfig* config =
        [[TipsModuleConfig alloc] initWithTipIdentifier:tips[i]];

    TipsModuleView* view = [[TipsModuleView alloc] initWithConfig:config];

    [_superview addSubview:view];

    ExpectSubview(identifiers[i], true);
  }
}

// Tests that the module displays the correct number of subviews for each tip
// type.
TEST_F(TipsModuleViewTest, DisplaysCorrectNumberOfSubviews) {
  // Currently, all tips display one item, i.e. the hero-cell default layout
  // item. If a new tip is added with more than one item, this test should be
  // updated.
  std::vector<TipIdentifier> tips = {
      TipIdentifier::kLensSearch,           TipIdentifier::kLensShop,
      TipIdentifier::kLensTranslate,        TipIdentifier::kAddressBarPosition,
      TipIdentifier::kSavePasswords,        TipIdentifier::kAutofillPasswords,
      TipIdentifier::kEnhancedSafeBrowsing,
  };

  for (TipIdentifier tip : tips) {
    TipsModuleConfig* config =
        [[TipsModuleConfig alloc] initWithTipIdentifier:tip];

    TipsModuleView* view = [[TipsModuleView alloc] initWithConfig:config];

    [_superview addSubview:view];

    ExpectUniqueSubview<IconDetailView>(_superview);
    ExpectUniqueSubview<IconView>(_superview);
    ExpectUniqueSubview<TipsModuleView>(_superview);

    [view removeFromSuperview];
  }
}
