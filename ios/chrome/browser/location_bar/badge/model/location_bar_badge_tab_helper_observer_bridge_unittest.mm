// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_tab_helper_observer_bridge.h"

#import <optional>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/fake_location_bar_badge_observer.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_tab_helper.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

class LocationBarBadgeTabHelperObserverBridgeTest : public PlatformTest {
 protected:
  LocationBarBadgeTabHelperObserverBridgeTest() {
    feature_list_.InitAndEnableFeature(kLocationBarBadgeMigration);
    LocationBarBadgeTabHelper::CreateForWebState(&web_state_);
  }
  LocationBarBadgeTabHelper* tab_helper() {
    return LocationBarBadgeTabHelper::FromWebState(&web_state_);
  }
  base::test::ScopedFeatureList feature_list_;
  web::FakeWebState web_state_;
};

// Verifies that the bridge forwards displayed badge changes to the Objective-C
// observer.
TEST_F(LocationBarBadgeTabHelperObserverBridgeTest,
       ForwardsDisplayedBadgeChangedToObserverWhenBadgeAddedAndRemoved) {
  FakeLocationBarBadgeObserver* observer =
      [[FakeLocationBarBadgeObserver alloc] init];
  LocationBarBadgeTabHelperObserverBridge bridge(tab_helper(), observer);

  tab_helper()->SetBadge(LocationBarBadgeType::kTranslate);
  EXPECT_EQ(observer.callCount, 1);

  tab_helper()->RemoveBadge(LocationBarBadgeType::kTranslate);
  EXPECT_EQ(observer.callCount, 2);
}

// Verifies that the bridge stops observing when destroyed.
TEST_F(LocationBarBadgeTabHelperObserverBridgeTest,
       StopsObservingWhenBridgeIsDestroyed) {
  FakeLocationBarBadgeObserver* observer =
      [[FakeLocationBarBadgeObserver alloc] init];
  std::optional<LocationBarBadgeTabHelperObserverBridge> bridge;
  bridge.emplace(tab_helper(), observer);

  tab_helper()->SetBadge(LocationBarBadgeType::kTranslate);
  EXPECT_EQ(observer.callCount, 1);

  bridge.reset();
  tab_helper()->RemoveBadge(LocationBarBadgeType::kTranslate);
  EXPECT_EQ(observer.callCount, 1);
}

// Verifies that the bridge resets its observation when the tab helper is
// destroyed.
TEST_F(LocationBarBadgeTabHelperObserverBridgeTest,
       ResetsObservationWhenTabHelperIsDestroyed) {
  auto web_state = std::make_unique<web::FakeWebState>();
  LocationBarBadgeTabHelper::CreateForWebState(web_state.get());
  LocationBarBadgeTabHelper* local_tab_helper =
      LocationBarBadgeTabHelper::FromWebState(web_state.get());

  FakeLocationBarBadgeObserver* observer =
      [[FakeLocationBarBadgeObserver alloc] init];
  LocationBarBadgeTabHelperObserverBridge bridge(local_tab_helper, observer);

  // Tab helper destruction should cleanly trigger
  // `LocationBarBadgeTabHelperDestroyed` and reset the bridge's observation.
  web_state.reset();
}
