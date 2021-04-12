// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"

#include "base/ios/ios_util.h"
#include "base/test/scoped_feature_list.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class DefaultBrowserUtilsTest : public PlatformTest {
 protected:
  void SetUp() override {
    const std::map<std::string, std::string> feature_params = {
        {"variant_ios_enabled", "true"},
        {"variant_safe_enabled", "true"},
        {"variant_tabs_enabled", "true"},
    };
    feature_list_.InitAndEnableFeatureWithParameters(kDefaultPromoTailored,
                                                     feature_params);
    ClearUserDefaults();
  }
  void TearDown() override { ClearUserDefaults(); }

  // Clear NSUserDefault keys used in the class.
  void ClearUserDefaults() {
    NSArray<NSString*>* keys = @[
      @"lastSignificantUserEvent", @"lastSignificantUserEventStaySafe",
      @"lastSignificantUserEventMadeForIOS", @"lastSignificantUserEventAllTabs"
    ];
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    for (NSString* key in keys) {
      [defaults removeObjectForKey:key];
    }
  }

  base::test::ScopedFeatureList feature_list_;
};

// Tests interesting information for each type.
TEST_F(DefaultBrowserUtilsTest, LogInterestingActivityEach) {
  if (!base::ios::IsRunningOnIOS14OrLater()) {
    // On iOS < 14 it should always be false.
    LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
    EXPECT_FALSE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeGeneral));
    LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);
    EXPECT_FALSE(
        IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeStaySafe));
    LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
    EXPECT_FALSE(
        IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeMadeForIOS));
    LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
    EXPECT_FALSE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeAllTabs));
    return;
  }

  // General promo.
  EXPECT_FALSE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeGeneral));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  EXPECT_TRUE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeGeneral));
  ClearUserDefaults();

  // Stay safe promo.
  EXPECT_FALSE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeStaySafe));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);
  EXPECT_TRUE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeStaySafe));
  ClearUserDefaults();

  // Made for iOS promo.
  EXPECT_FALSE(
      IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeMadeForIOS));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  EXPECT_TRUE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeMadeForIOS));
  ClearUserDefaults();

  // All tabs promo.
  EXPECT_FALSE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeAllTabs));
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  EXPECT_TRUE(IsLikelyInterestedDefaultBrowserUser(DefaultPromoTypeAllTabs));
}

// Tests interesting information for any type.
TEST_F(DefaultBrowserUtilsTest, LogInterestingActivityAny) {
  if (!base::ios::IsRunningOnIOS14OrLater()) {
    // On iOS < 14 it should always be false.
    LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
    LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);
    LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
    LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
    EXPECT_FALSE(IsLikelyInterestedDefaultBrowserUser());
    return;
  }

  // General log.
  EXPECT_FALSE(IsLikelyInterestedDefaultBrowserUser());
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  EXPECT_TRUE(IsLikelyInterestedDefaultBrowserUser());
  ClearUserDefaults();

  // Stay safe log.
  EXPECT_FALSE(IsLikelyInterestedDefaultBrowserUser());
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);
  EXPECT_TRUE(IsLikelyInterestedDefaultBrowserUser());
  ClearUserDefaults();

  // Made for iOS log.
  EXPECT_FALSE(IsLikelyInterestedDefaultBrowserUser());
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  EXPECT_TRUE(IsLikelyInterestedDefaultBrowserUser());
  ClearUserDefaults();

  // All tabs log.
  EXPECT_FALSE(IsLikelyInterestedDefaultBrowserUser());
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  EXPECT_TRUE(IsLikelyInterestedDefaultBrowserUser());
}

}  // namespace
