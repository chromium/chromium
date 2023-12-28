// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/ios_chrome_synced_tab_delegate.h"

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "components/sync/base/features.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using IOSChromeSyncedTabDelegateTest = PlatformTest;

// Regression test for crbug.com/980841: Verifies that the browser does not
// crash if the pending item is null.
TEST_F(IOSChromeSyncedTabDelegateTest, ShouldHandleNullItem) {
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  navigation_manager->SetPendingItemIndex(0);

  ASSERT_EQ(0, navigation_manager->GetPendingItemIndex());
  ASSERT_EQ(nullptr, navigation_manager->GetPendingItem());

  web::FakeWebState web_state;
  web_state.SetNavigationManager(std::move(navigation_manager));
  IOSChromeSyncedTabDelegate::CreateForWebState(&web_state);

  IOSChromeSyncedTabDelegate* tab_delegate =
      IOSChromeSyncedTabDelegate::FromWebState(&web_state);

  EXPECT_EQ(GURL(), tab_delegate->GetVirtualURLAtIndex(0));
}

// Tests that GetLastActiveTime() is returning the cached value if less time
// than a threshold has passed, and is returning the WebState last active time
// if more time has passed.
TEST_F(IOSChromeSyncedTabDelegateTest, CachedLastActiveTime) {
  base::TimeDelta threshold = base::Minutes(3);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{syncer::kSyncSessionOnVisibilityChanged,
        {{"SyncSessionOnVisibilityChangedTimeThreshold",
          base::NumberToString(threshold.InMinutes()) + "m"}}}},
      /*disabled_features=*/{});

  web::FakeWebState web_state;
  IOSChromeSyncedTabDelegate::CreateForWebState(&web_state);

  IOSChromeSyncedTabDelegate* tab_delegate =
      IOSChromeSyncedTabDelegate::FromWebState(&web_state);

  base::Time original_time = base::Time::Now();
  web_state.SetLastActiveTime(original_time);

  EXPECT_EQ(original_time, tab_delegate->GetLastActiveTime());

  // If not enough time has passed, the cached time should be returned.
  base::Time before_threshold = original_time + threshold - base::Minutes(1);
  web_state.SetLastActiveTime(before_threshold);
  EXPECT_EQ(original_time, tab_delegate->GetLastActiveTime());

  // After the threshold has passed, the new value should be returned.
  base::Time after_threshold = original_time + threshold + base::Minutes(1);
  web_state.SetLastActiveTime(after_threshold);
  EXPECT_EQ(after_threshold, tab_delegate->GetLastActiveTime());
}

// Tests that the resetting the cached value of last_active_time allows to
// return the value from the WebState even if less time than the threshold has
// passed.
TEST_F(IOSChromeSyncedTabDelegateTest, ResetCachedLastActiveTime) {
  base::TimeDelta threshold = base::Minutes(3);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{syncer::kSyncSessionOnVisibilityChanged,
        {{"SyncSessionOnVisibilityChangedTimeThreshold",
          base::NumberToString(threshold.InMinutes()) + "m"}}}},
      /*disabled_features=*/{});

  web::FakeWebState web_state;
  IOSChromeSyncedTabDelegate::CreateForWebState(&web_state);

  IOSChromeSyncedTabDelegate* tab_delegate =
      IOSChromeSyncedTabDelegate::FromWebState(&web_state);

  base::Time original_time = base::Time::Now();
  web_state.SetLastActiveTime(original_time);

  EXPECT_EQ(original_time, tab_delegate->GetLastActiveTime());

  tab_delegate->ResetCachedLastActiveTime();

  // Even if the threshold is not passed, the cached value has been reset so the
  // new time should be returned.
  base::Time before_threshold = original_time + threshold - base::Minutes(1);
  web_state.SetLastActiveTime(before_threshold);
  EXPECT_EQ(before_threshold, tab_delegate->GetLastActiveTime());
}

}  // namespace
