// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_view.h"

#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_view+testing.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using PriceTrackingPromoViewTest = PlatformTest;

TEST_F(PriceTrackingPromoViewTest, TestTitle) {
  PriceTrackingPromoModuleView* view =
      [[PriceTrackingPromoModuleView alloc] initWithFrame:CGRectZero];
  EXPECT_NSEQ(@"Get price tracking notifications",
              view.titleLabelTextForTesting);
}

TEST_F(PriceTrackingPromoViewTest, TestDescription) {
  PriceTrackingPromoModuleView* view =
      [[PriceTrackingPromoModuleView alloc] initWithFrame:CGRectZero];
  EXPECT_NSEQ(@"Keep up with price drops on all the products you track.",
              view.descriptionLabelTextForTesting);
}

TEST_F(PriceTrackingPromoViewTest, TestAllow) {
  PriceTrackingPromoModuleView* view =
      [[PriceTrackingPromoModuleView alloc] initWithFrame:CGRectZero];
  EXPECT_TRUE([view.allowLabelTextForTesting
      containsString:@"Allow price tracking notifications"]);
}
