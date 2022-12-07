// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"

#import "base/ios/ios_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils_test_support.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class DefaultBrowserUtilsTest : public PlatformTest {
 protected:
  void SetUp() override { ClearDefaultBrowserPromoData(); }
  void TearDown() override { ClearDefaultBrowserPromoData(); }

  base::test::ScopedFeatureList feature_list_;
};

// Tests interesting information for each type.
TEST_F(DefaultBrowserUtilsTest, LogInterestingActivityEach) {

  // General promo.
  EXPECT_FALSE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeGeneral));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  EXPECT_TRUE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeGeneral));
  ClearDefaultBrowserPromoData();

  // Stay safe promo.
  EXPECT_FALSE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeStaySafe));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);
  EXPECT_TRUE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeStaySafe));
  ClearDefaultBrowserPromoData();

  // Made for iOS promo.
  EXPECT_FALSE(
      IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeMadeForIOS));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  EXPECT_TRUE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeMadeForIOS));
  ClearDefaultBrowserPromoData();

  // All tabs promo.
  EXPECT_FALSE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeAllTabs));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  EXPECT_TRUE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeAllTabs));
}

// Tests most recent interest type.
TEST_F(DefaultBrowserUtilsTest, MostRecentInterestDefaultPromoType) {
  DefaultPromoType type = MostRecentInterestDefaultPromoType(NO);
  EXPECT_EQ(type, DefaultPromoTypeGeneral);

  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  type = MostRecentInterestDefaultPromoType(NO);
  EXPECT_EQ(type, DefaultPromoTypeAllTabs);
  type = MostRecentInterestDefaultPromoType(YES);
  EXPECT_NE(type, DefaultPromoTypeAllTabs);

  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);
  type = MostRecentInterestDefaultPromoType(NO);
  EXPECT_EQ(type, DefaultPromoTypeStaySafe);

  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  type = MostRecentInterestDefaultPromoType(NO);
  EXPECT_EQ(type, DefaultPromoTypeMadeForIOS);
}

// Tests cool down between promos.
TEST_F(DefaultBrowserUtilsTest, PromoCoolDown) {
  LogUserInteractionWithFullscreenPromo();
  EXPECT_TRUE(UserInPromoCooldown());

  ClearDefaultBrowserPromoData();
  LogUserInteractionWithTailoredFullscreenPromo();
  EXPECT_TRUE(UserInPromoCooldown());
}

// Tests no 2 tailored promos are not shown.
TEST_F(DefaultBrowserUtilsTest, TailoredPromoDoesNotAppearTwoTimes) {
  LogUserInteractionWithTailoredFullscreenPromo();
  EXPECT_TRUE(HasUserInteractedWithTailoredFullscreenPromoBefore());
}

}  // namespace
