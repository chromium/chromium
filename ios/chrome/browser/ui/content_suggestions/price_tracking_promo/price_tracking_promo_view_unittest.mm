// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_view.h"

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_item.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_view+testing.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

PriceTrackingPromoModuleView* GetConfiguredPriceTrackingPromoModuleView() {
  PriceTrackingPromoModuleView* view =
      [[PriceTrackingPromoModuleView alloc] initWithFrame:CGRectZero];
  PriceTrackingPromoItem* item = [[PriceTrackingPromoItem alloc] init];
  [view configureView:item];
  return view;
}

}  // namespace

using PriceTrackingPromoViewTest = PlatformTest;

TEST_F(PriceTrackingPromoViewTest, TestTitle) {
  PriceTrackingPromoModuleView* view =
      GetConfiguredPriceTrackingPromoModuleView();
  EXPECT_NSEQ(@"Get price tracking notifications",
              view.titleLabelTextForTesting);
}

TEST_F(PriceTrackingPromoViewTest, TestDescription) {
  PriceTrackingPromoModuleView* view =
      GetConfiguredPriceTrackingPromoModuleView();
  EXPECT_NSEQ(@"Keep up with price drops on all the products you track.",
              view.descriptionLabelTextForTesting);
}

TEST_F(PriceTrackingPromoViewTest, TestAllow) {
  PriceTrackingPromoModuleView* view =
      GetConfiguredPriceTrackingPromoModuleView();
  EXPECT_TRUE([view.allowLabelTextForTesting
      containsString:@"Allow price tracking notifications"]);
}
