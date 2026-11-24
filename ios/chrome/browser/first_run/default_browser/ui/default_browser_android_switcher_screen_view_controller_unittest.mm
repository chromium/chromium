// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_android_switcher_screen_view_controller.h"

#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Unit tests for `DefaultBrowserAndroidSwitcherScreenViewController`.
class DefaultBrowserAndroidSwitcherScreenViewControllerTest
    : public PlatformTest {
 protected:
  // Returns the view of a view controller showing `lens_slot_feature`.
  UIView* LoadViewWithLensSlotFeature(
      AndroidSwitcherLensSlotFeature lens_slot_feature) {
    view_controller_ =
        [[DefaultBrowserAndroidSwitcherScreenViewController alloc] init];
    [view_controller_ setLensSlotFeature:lens_slot_feature];
    return view_controller_.view;
  }

  DefaultBrowserAndroidSwitcherScreenViewController* view_controller_;
};

// Tests that the Lens row is shown instead of the Safe Browsing row when the
// Lens slot feature is Lens.
TEST_F(DefaultBrowserAndroidSwitcherScreenViewControllerTest,
       ShowsLensRowForLensFeature) {
  UIView* view =
      LoadViewWithLensSlotFeature(AndroidSwitcherLensSlotFeature::kLens);

  EXPECT_TRUE(chrome_test_util::FindLabelWithText(
      view, l10n_util::GetNSString(
                IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_LENS_ROW)));
  EXPECT_FALSE(chrome_test_util::FindLabelWithText(
      view,
      l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_SAFE_BROWSING_ROW)));
}

// Tests that the Safe Browsing row is shown instead of the Lens row when the
// Lens slot feature is Safe Browsing.
TEST_F(DefaultBrowserAndroidSwitcherScreenViewControllerTest,
       ShowsSafeBrowsingRowForSafeBrowsingFeature) {
  UIView* view = LoadViewWithLensSlotFeature(
      AndroidSwitcherLensSlotFeature::kSafeBrowsing);

  EXPECT_TRUE(chrome_test_util::FindLabelWithText(
      view,
      l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_SAFE_BROWSING_ROW)));
  EXPECT_FALSE(chrome_test_util::FindLabelWithText(
      view, l10n_util::GetNSString(
                IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SWITCHER_LENS_ROW)));
}

}  // namespace
