// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/promo/contextual/ui/contextual_default_browser_promo_view_controller.h"

#import "ios/chrome/browser/default_browser/promo/contextual/public/contextual_default_browser_promo_constants.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

class ContextualDefaultBrowserPromoViewControllerTest : public PlatformTest {
 protected:
  ContextualDefaultBrowserPromoViewControllerTest() {
    view_controller_ =
        [[ContextualDefaultBrowserPromoViewController alloc] init];
  }

  ContextualDefaultBrowserPromoViewController* view_controller_;
  ScopedKeyWindow scoped_key_window_;
};

// Tests that setting consumer properties updates the view controller properly
// and that default action strings are loaded on view presentation.
TEST_F(ContextualDefaultBrowserPromoViewControllerTest,
       TestConsumerProperties) {
  NSString* title = @"Test Title";
  NSString* subtitle = @"Test Subtitle";

  [view_controller_ setPromoTitle:title];
  [view_controller_ setPromoSubtitle:subtitle];
  [view_controller_ setAnimationAssetName:@"FRE_Summarize_Slide"];
  [view_controller_ setLightModeColorProvider:@{@"key" : [UIColor blackColor]}
                        darkModeColorProvider:@{@"key" : [UIColor whiteColor]}];

  [scoped_key_window_.Get() setRootViewController:view_controller_];

  EXPECT_NSEQ(title, view_controller_.titleString);
  EXPECT_NSEQ(subtitle, view_controller_.subtitleString);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_DEFAULT_BROWSER_CONTEXTUAL_PRIMARY_BUTTON_TEXT),
              view_controller_.configuration.primaryActionString);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_DEFAULT_BROWSER_CONTEXTUAL_SECONDARY_BUTTON_TEXT),
              view_controller_.configuration.secondaryActionString);
  EXPECT_NSEQ(kContextualDefaultBrowserPromoAccessibilityIdentifier,
              view_controller_.view.accessibilityIdentifier);
}
