// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/price_tracking_promo/ui/price_tracking_promo_module_view.h"

#import "ios/chrome/browser/content_suggestions/ui_bundled/price_tracking_promo/ui/price_tracking_promo_item.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/price_tracking_promo/ui/price_tracking_promo_module_view+testing.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Helper function to create and configure a PriceTrackingPromoModuleView
// with a default PriceTrackingPromoItem for testing.
PriceTrackingPromoModuleView* GetConfiguredPriceTrackingPromoModuleView() {
  PriceTrackingPromoModuleView* view =
      [[PriceTrackingPromoModuleView alloc] initWithFrame:CGRectZero];
  PriceTrackingPromoItem* item = [[PriceTrackingPromoItem alloc] init];
  [view configureView:item];
  return view;
}

}  // namespace

// Test fixture for `PriceTrackingPromoModuleView`.
using PriceTrackingPromoModuleViewTest = PlatformTest;

// Tests that the module view displays the correct title text.
TEST_F(PriceTrackingPromoModuleViewTest, TestTitle) {
  PriceTrackingPromoModuleView* view =
      GetConfiguredPriceTrackingPromoModuleView();
  EXPECT_NSEQ(@"Get price tracking notifications",
              view.titleLabelTextForTesting);
}

// Tests that the module view displays the correct description text.
TEST_F(PriceTrackingPromoModuleViewTest, TestDescription) {
  PriceTrackingPromoModuleView* view =
      GetConfiguredPriceTrackingPromoModuleView();
  EXPECT_NSEQ(@"Keep up with price drops on all the products you track.",
              view.descriptionLabelTextForTesting);
}

// Tests that the "Allow" text within the module view contains the expected
// string.
TEST_F(PriceTrackingPromoModuleViewTest, TestAllow) {
  PriceTrackingPromoModuleView* view =
      GetConfiguredPriceTrackingPromoModuleView();
  EXPECT_TRUE([view.allowLabelTextForTesting
      containsString:@"Allow price tracking notifications"]);
}

// Tests the layout constraints related to the favicon when no product image is
// shown. This test verifies that calling
// `addConstraintsForProductImageForTesting` in this state does not cause
// issues.
TEST_F(PriceTrackingPromoModuleViewTest, TestFaviconWhenNoProductImage) {
  PriceTrackingPromoModuleView* view =
      GetConfiguredPriceTrackingPromoModuleView();
  [view addConstraintsForProductImageForTesting];
}
