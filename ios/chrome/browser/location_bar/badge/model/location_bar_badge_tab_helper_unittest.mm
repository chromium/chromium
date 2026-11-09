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
  std::optional<LocationBarBadgeType> GetDisplayedBadgeType() {
    std::optional<LocationBarBadgeRequest> request =
        tab_helper()->GetDisplayedBadgeRequest();
    return request ? std::optional(request->badge_type) : std::nullopt;
  }
  base::test::ScopedFeatureList feature_list_;
  web::FakeWebState web_state_;
};

// Tests adding, duplicate checking, and removing badges updates observers.
TEST_F(LocationBarBadgeTabHelperTest,
       UpdatesDisplayedBadgeAndNotifiesObserverWhenBadgeAddedAndRemoved) {
  FakeLocationBarBadgeTabHelperObserver observer;
  tab_helper()->AddObserver(&observer);
  EXPECT_FALSE(tab_helper()->GetDisplayedBadgeRequest().has_value());

  tab_helper()->SetBadge({.badge_type = LocationBarBadgeType::kPasswordSave});
  EXPECT_TRUE(tab_helper()->HasBadge(LocationBarBadgeType::kPasswordSave));
  EXPECT_EQ(GetDisplayedBadgeType(), LocationBarBadgeType::kPasswordSave);
  EXPECT_EQ(observer.call_count(), 1);

  // Redundant insertion with an identical request does not notify.
  tab_helper()->SetBadge({.badge_type = LocationBarBadgeType::kPasswordSave});
  EXPECT_EQ(observer.call_count(), 1);

  tab_helper()->RemoveBadge(LocationBarBadgeType::kPasswordSave);
  EXPECT_FALSE(tab_helper()->HasBadge(LocationBarBadgeType::kPasswordSave));
  EXPECT_FALSE(tab_helper()->GetDisplayedBadgeRequest().has_value());
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
  tab_helper()->SetBadge({.badge_type = LocationBarBadgeType::kPasswordSave});
  EXPECT_EQ(observer.call_count(), 1);
  EXPECT_EQ(GetDisplayedBadgeType(), LocationBarBadgeType::kPasswordSave);

  // Add a secondary badge sharing the same priority. Incumbent retains
  // precedence.
  tab_helper()->SetBadge({.badge_type = LocationBarBadgeType::kSaveCard});
  EXPECT_TRUE(tab_helper()->HasBadge(LocationBarBadgeType::kSaveCard));
  EXPECT_EQ(GetDisplayedBadgeType(), LocationBarBadgeType::kPasswordSave);
  EXPECT_EQ(observer.call_count(), 1);

  // Updating attributes on a non-displayed badge does not notify observers.
  tab_helper()->SetBadge({
      .badge_type = LocationBarBadgeType::kSaveCard,
      .accepted = true,
  });
  EXPECT_EQ(observer.call_count(), 1);

  // Updating attributes on the displayed badge notifies observers while
  // keeping its incumbent precedence.
  tab_helper()->SetBadge({
      .badge_type = LocationBarBadgeType::kPasswordSave,
      .accepted = true,
  });
  EXPECT_EQ(observer.call_count(), 2);
  EXPECT_EQ(GetDisplayedBadgeType(), LocationBarBadgeType::kPasswordSave);

  // Remove secondary badge while displayed badge remains kPasswordSave.
  tab_helper()->RemoveBadge(LocationBarBadgeType::kSaveCard);
  EXPECT_FALSE(tab_helper()->HasBadge(LocationBarBadgeType::kSaveCard));
  EXPECT_EQ(GetDisplayedBadgeType(), LocationBarBadgeType::kPasswordSave);
  EXPECT_EQ(observer.call_count(), 2);

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

// Tests higher priority badges preempt lower priority badges, and promoting a
// lower-priority badge does not preempt the incumbent at the higher tier.
TEST_F(LocationBarBadgeTabHelperTest,
       PreemptsLowerTierWhenHigherPriorityBadgeAdded) {
  tab_helper()->SetBadge({.badge_type = LocationBarBadgeType::kTranslate});
  EXPECT_EQ(GetDisplayedBadgeType(), LocationBarBadgeType::kTranslate);

  tab_helper()->SetBadge({
      .badge_type = LocationBarBadgeType::kPasswordSave,
      .priority = LocationBarBadgePriority::kHigh,
  });
  EXPECT_EQ(GetDisplayedBadgeType(), LocationBarBadgeType::kPasswordSave);

  // Promoting kTranslate to kHigh gets a newer sequence number and does not
  // preempt the kHigh incumbent (kPasswordSave).
  tab_helper()->SetBadge({
      .badge_type = LocationBarBadgeType::kTranslate,
      .priority = LocationBarBadgePriority::kHigh,
  });
  EXPECT_EQ(GetDisplayedBadgeType(), LocationBarBadgeType::kPasswordSave);

  tab_helper()->RemoveBadge(LocationBarBadgeType::kPasswordSave);
  EXPECT_EQ(GetDisplayedBadgeType(), LocationBarBadgeType::kTranslate);
}

// Tests arrival order preserves the incumbent badge for equal priority tiers.
TEST_F(LocationBarBadgeTabHelperTest,
       PreservesIncumbentWhenBadgesShareSamePriority) {
  tab_helper()->SetBadge({.badge_type = LocationBarBadgeType::kTranslate});
  tab_helper()->SetBadge({.badge_type = LocationBarBadgeType::kSaveCard});
  EXPECT_EQ(GetDisplayedBadgeType(), LocationBarBadgeType::kTranslate);
}

// Tests non-persistent badges are preserved on same-document navigation.
TEST_F(LocationBarBadgeTabHelperTest, PreservesBadgesOnSameDocumentNavigation) {
  tab_helper()->SetBadge({.badge_type = LocationBarBadgeType::kTranslate});

  web::FakeNavigationContext navigation_context;
  navigation_context.SetIsSameDocument(true);
  web_state_.OnNavigationStarted(&navigation_context);

  EXPECT_TRUE(tab_helper()->HasBadge(LocationBarBadgeType::kTranslate));
  EXPECT_EQ(GetDisplayedBadgeType(), LocationBarBadgeType::kTranslate);
}

// Tests non-persistent badges are cleared across cross-document navigation.
TEST_F(LocationBarBadgeTabHelperTest,
       ClearsNonPersistentBadgesOnCrossDocumentNavigation) {
  tab_helper()->SetBadge({.badge_type = LocationBarBadgeType::kTranslate});
  tab_helper()->SetBadge({.badge_type = LocationBarBadgeType::kIncognito});

  web::FakeNavigationContext navigation_context;
  navigation_context.SetIsSameDocument(false);
  web_state_.OnNavigationStarted(&navigation_context);

  EXPECT_FALSE(tab_helper()->HasBadge(LocationBarBadgeType::kTranslate));
  EXPECT_TRUE(tab_helper()->HasBadge(LocationBarBadgeType::kIncognito));
  EXPECT_EQ(GetDisplayedBadgeType(), LocationBarBadgeType::kIncognito);
}

// Tests that `LocationBarBadgeRequest` preserves all configuration attributes
// when stored and retrieved via `LocationBarBadgeTabHelper`.
TEST_F(LocationBarBadgeTabHelperTest,
       ReturnsConfiguredBadgeRequestWhenSetOnTabHelper) {
  const LocationBarBadgeRequest reader_mode_request = {
      .badge_type = LocationBarBadgeType::kReaderMode,
      .priority = LocationBarBadgePriority::kDefault,
      .accessibility_label = u"Reader Mode Available",
      .accessibility_identifier = "ReaderModeIdentifier",
      .label_text = u"Reader Mode",
      .has_unread_indicator = true,
      .accepted = true,
      .is_highlighted = true,
  };
  tab_helper()->SetBadge(reader_mode_request);
  const std::optional<LocationBarBadgeRequest> displayed_request =
      tab_helper()->GetDisplayedBadgeRequest();
  ASSERT_TRUE(displayed_request.has_value());
  EXPECT_EQ(*displayed_request, reader_mode_request);
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

  tab_helper()->SetBadge({.badge_type = LocationBarBadgeType::kPasswordSave});
  EXPECT_FALSE(tab_helper()->HasBadge(LocationBarBadgeType::kPasswordSave));
  EXPECT_FALSE(tab_helper()->GetDisplayedBadgeRequest().has_value());
  EXPECT_EQ(observer.call_count(), 0);

  tab_helper()->RemoveBadge(LocationBarBadgeType::kPasswordSave);
  EXPECT_FALSE(tab_helper()->HasBadge(LocationBarBadgeType::kPasswordSave));
  EXPECT_FALSE(tab_helper()->GetDisplayedBadgeRequest().has_value());
  EXPECT_EQ(observer.call_count(), 0);

  tab_helper()->RemoveObserver(&observer);
}
