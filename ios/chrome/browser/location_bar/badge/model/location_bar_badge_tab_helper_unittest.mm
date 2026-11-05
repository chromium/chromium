// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_tab_helper.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/fake_location_bar_badge_tab_helper_observer.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

class LocationBarBadgeTabHelperTest : public PlatformTest {
 protected:
  LocationBarBadgeTabHelperTest() {
    feature_list_.InitAndEnableFeature(kLocationBarBadgeMigration);
    LocationBarBadgeTabHelper::CreateForWebState(&web_state_);
  }
  LocationBarBadgeTabHelper* tab_helper() {
    return LocationBarBadgeTabHelper::FromWebState(&web_state_);
  }
  base::test::ScopedFeatureList feature_list_;
  web::FakeWebState web_state_;
};

// Tests adding, duplicate checking, and removing badges updates observers.
TEST_F(LocationBarBadgeTabHelperTest,
       UpdatesDisplayedBadgeAndNotifiesObserverWhenBadgeAddedAndRemoved) {
  FakeLocationBarBadgeTabHelperObserver observer;
  tab_helper()->AddObserver(&observer);
  EXPECT_FALSE(tab_helper()->GetDisplayedBadge().has_value());

  tab_helper()->SetBadge(LocationBarBadgeType::kPasswordSave);
  EXPECT_TRUE(tab_helper()->HasBadge(LocationBarBadgeType::kPasswordSave));
  EXPECT_EQ(tab_helper()->GetDisplayedBadge(),
            LocationBarBadgeType::kPasswordSave);
  EXPECT_EQ(observer.call_count(), 1);

  // Redundant insertion at same priority does not notify.
  tab_helper()->SetBadge(LocationBarBadgeType::kPasswordSave);
  EXPECT_EQ(observer.call_count(), 1);

  tab_helper()->RemoveBadge(LocationBarBadgeType::kPasswordSave);
  EXPECT_FALSE(tab_helper()->HasBadge(LocationBarBadgeType::kPasswordSave));
  EXPECT_FALSE(tab_helper()->GetDisplayedBadge().has_value());
  EXPECT_EQ(observer.call_count(), 2);
  tab_helper()->RemoveObserver(&observer);
}

// Verifies that removing an inactive badge does not notify observers.
TEST_F(LocationBarBadgeTabHelperTest,
       DoesNotNotifyObserverWhenRemovingNonActiveBadge) {
  FakeLocationBarBadgeTabHelperObserver observer;
  tab_helper()->AddObserver(&observer);

  tab_helper()->RemoveBadge(LocationBarBadgeType::kPasswordSave);
  EXPECT_EQ(observer.call_count(), 0);

  tab_helper()->RemoveObserver(&observer);
}

// Verifies that mutations that do not change the displayed badge do not
// spuriously notify observers.
TEST_F(LocationBarBadgeTabHelperTest,
       DoesNotNotifyObserverWhenDisplayedBadgeUnchanged) {
  FakeLocationBarBadgeTabHelperObserver observer;
  tab_helper()->AddObserver(&observer);

  // Add the incumbent badge at default priority.
  tab_helper()->SetBadge(LocationBarBadgeType::kPasswordSave,
                         LocationBarBadgePriority::kDefault);
  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_EQ(tab_helper()->GetDisplayedBadge(),
            LocationBarBadgeType::kPasswordSave);

  // Add a secondary badge sharing the same priority. Incumbent retains
  // precedence.
  tab_helper()->SetBadge(LocationBarBadgeType::kSaveCard,
                         LocationBarBadgePriority::kDefault);
  EXPECT_TRUE(tab_helper()->HasBadge(LocationBarBadgeType::kSaveCard));
  EXPECT_EQ(tab_helper()->GetDisplayedBadge(),
            LocationBarBadgeType::kPasswordSave);
  EXPECT_EQ(observer.call_count(), 1);

  // Remove secondary badge while displayed badge remains kPasswordSave.
  tab_helper()->RemoveBadge(LocationBarBadgeType::kSaveCard);
  EXPECT_FALSE(tab_helper()->HasBadge(LocationBarBadgeType::kSaveCard));
  EXPECT_EQ(tab_helper()->GetDisplayedBadge(),
            LocationBarBadgeType::kPasswordSave);
  EXPECT_EQ(observer.call_count(), 1);

  tab_helper()->RemoveObserver(&observer);
}

// Verifies that observers are notified when the tab helper is destroyed.
TEST_F(LocationBarBadgeTabHelperTest,
       NotifiesObserverWhenTabHelperIsDestroyed) {
  auto web_state = std::make_unique<web::FakeWebState>();
  LocationBarBadgeTabHelper::CreateForWebState(web_state.get());
  LocationBarBadgeTabHelper* local_tab_helper =
      LocationBarBadgeTabHelper::FromWebState(web_state.get());

  FakeLocationBarBadgeTabHelperObserver local_observer;
  local_tab_helper->AddObserver(&local_observer);
  EXPECT_FALSE(local_observer.tab_helper_destroyed());

  web_state.reset();
  EXPECT_TRUE(local_observer.tab_helper_destroyed());
}

// Tests higher priority badges preempt lower priority badges.
TEST_F(LocationBarBadgeTabHelperTest,
       PreemptsLowerTierWhenHigherPriorityBadgeAdded) {
  tab_helper()->SetBadge(LocationBarBadgeType::kTranslate,
                         LocationBarBadgePriority::kDefault);
  EXPECT_EQ(tab_helper()->GetDisplayedBadge(),
            LocationBarBadgeType::kTranslate);

  tab_helper()->SetBadge(LocationBarBadgeType::kPasswordSave,
                         LocationBarBadgePriority::kHigh);
  EXPECT_EQ(tab_helper()->GetDisplayedBadge(),
            LocationBarBadgeType::kPasswordSave);

  tab_helper()->RemoveBadge(LocationBarBadgeType::kPasswordSave);
  EXPECT_EQ(tab_helper()->GetDisplayedBadge(),
            LocationBarBadgeType::kTranslate);
}

// Tests arrival order preserves the incumbent badge for equal priority tiers.
TEST_F(LocationBarBadgeTabHelperTest,
       PreservesIncumbentWhenBadgesShareSamePriority) {
  tab_helper()->SetBadge(LocationBarBadgeType::kTranslate,
                         LocationBarBadgePriority::kDefault);
  tab_helper()->SetBadge(LocationBarBadgeType::kSaveCard,
                         LocationBarBadgePriority::kDefault);
  EXPECT_EQ(tab_helper()->GetDisplayedBadge(),
            LocationBarBadgeType::kTranslate);
}

// Tests non-persistent badges are preserved on same-document navigation.
TEST_F(LocationBarBadgeTabHelperTest, PreservesBadgesOnSameDocumentNavigation) {
  tab_helper()->SetBadge(LocationBarBadgeType::kTranslate,
                         LocationBarBadgePriority::kDefault);

  web::FakeNavigationContext navigation_context;
  navigation_context.SetIsSameDocument(true);
  web_state_.OnNavigationStarted(&navigation_context);

  EXPECT_TRUE(tab_helper()->HasBadge(LocationBarBadgeType::kTranslate));
  EXPECT_EQ(tab_helper()->GetDisplayedBadge(),
            LocationBarBadgeType::kTranslate);
}

// Tests non-persistent badges are cleared across cross-document navigation.
TEST_F(LocationBarBadgeTabHelperTest,
       ClearsNonPersistentBadgesOnCrossDocumentNavigation) {
  tab_helper()->SetBadge(LocationBarBadgeType::kTranslate,
                         LocationBarBadgePriority::kDefault);
  tab_helper()->SetBadge(LocationBarBadgeType::kIncognito,
                         LocationBarBadgePriority::kDefault);

  web::FakeNavigationContext navigation_context;
  navigation_context.SetIsSameDocument(false);
  web_state_.OnNavigationStarted(&navigation_context);

  EXPECT_FALSE(tab_helper()->HasBadge(LocationBarBadgeType::kTranslate));
  EXPECT_TRUE(tab_helper()->HasBadge(LocationBarBadgeType::kIncognito));
  EXPECT_EQ(tab_helper()->GetDisplayedBadge(),
            LocationBarBadgeType::kIncognito);
}

// Test fixture with kLocationBarBadgeMigration feature disabled.
class LocationBarBadgeTabHelperFeatureDisabledTest : public PlatformTest {
 protected:
  LocationBarBadgeTabHelperFeatureDisabledTest() {
    feature_list_.InitAndDisableFeature(kLocationBarBadgeMigration);
    LocationBarBadgeTabHelper::CreateForWebState(&web_state_);
  }
  LocationBarBadgeTabHelper* tab_helper() {
    return LocationBarBadgeTabHelper::FromWebState(&web_state_);
  }
  base::test::ScopedFeatureList feature_list_;
  web::FakeWebState web_state_;
};

// Verifies that badge mutations are no-ops when the migration feature is
// disabled.
TEST_F(LocationBarBadgeTabHelperFeatureDisabledTest,
       BadgeMutationsAreNoOpsWhenFeatureDisabled) {
  FakeLocationBarBadgeTabHelperObserver observer;
  tab_helper()->AddObserver(&observer);

  tab_helper()->SetBadge(LocationBarBadgeType::kPasswordSave);
  EXPECT_FALSE(tab_helper()->HasBadge(LocationBarBadgeType::kPasswordSave));
  EXPECT_FALSE(tab_helper()->GetDisplayedBadge().has_value());
  EXPECT_EQ(observer.call_count(), 0);

  tab_helper()->RemoveBadge(LocationBarBadgeType::kPasswordSave);
  EXPECT_FALSE(tab_helper()->HasBadge(LocationBarBadgeType::kPasswordSave));
  EXPECT_FALSE(tab_helper()->GetDisplayedBadge().has_value());
  EXPECT_EQ(observer.call_count(), 0);

  tab_helper()->RemoveObserver(&observer);
}
