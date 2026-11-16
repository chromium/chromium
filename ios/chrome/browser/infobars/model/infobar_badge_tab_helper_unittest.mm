// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper.h"

#import "base/check.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_item.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_ios.h"
#import "ios/chrome/browser/infobars/ui_bundled/test_infobar_badge_tab_helper_delegate.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_request.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_tab_helper.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

namespace {
// The InfobarTypes to use for the test.
const InfobarType kInfobarTypeWithBadge = InfobarType::kInfobarTypePasswordSave;
const InfobarType kInfobarTypeWithMigratedBadge =
    InfobarType::kInfobarTypeTranslate;
const InfobarType kInfobarTypeNoBadge = InfobarType::kInfobarTypeConfirm;
}  // namespace

// Test fixture for testing InfobarBadgeTabHelper.
class InfobarBadgeTabHelperTest : public PlatformTest {
 protected:
  InfobarBadgeTabHelperTest()
      : delegate_([[TestInfobarTabHelperDelegate alloc] init]) {
    // Setup navigation manager. Needed for InfobarManager.
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());

    // Create the InfobarManager for web_state_.
    InfoBarManagerImpl::CreateForWebState(&web_state_);

    // Create the InfobarBadgeTabHelper for web_state_ and set its delegate.
    InfobarBadgeTabHelper::CreateForWebState(&web_state_);
    delegate_.badgeTabHelper = tab_helper();
    tab_helper()->SetDelegate(delegate_);
  }

  // Adds a FakeInfobarIOS with specified badge support to the WebState's
  // InfoBarManagerImpl.  Set replace_existing to true, if a matching infobar
  // (same message_text) should be replaced with new one instead of being
  // ignored. Returns the added infobar.
  FakeInfobarIOS* AddInfobar(bool has_badge, bool replace_existing = false) {
    std::unique_ptr<FakeInfobarIOS> added_infobar =
        std::make_unique<FakeInfobarIOS>(has_badge ? kInfobarTypeWithBadge
                                                   : kInfobarTypeNoBadge);
    FakeInfobarIOS* infobar = added_infobar.get();
    InfoBarManagerImpl::FromWebState(&web_state_)
        ->AddInfoBar(std::move(added_infobar), replace_existing);
    return infobar;
  }

  // Adds a `FakeInfobarIOS` with `infobar_type` to `web_state_`.
  FakeInfobarIOS* AddInfobarWithType(InfobarType infobar_type) {
    std::unique_ptr<FakeInfobarIOS> added_infobar =
        std::make_unique<FakeInfobarIOS>(infobar_type);
    FakeInfobarIOS* infobar = added_infobar.get();
    InfoBarManagerImpl::FromWebState(&web_state_)
        ->AddInfoBar(std::move(added_infobar), /*replace_existing=*/false);
    return infobar;
  }

  // Recreates `InfobarBadgeTabHelper` (under the active `ScopedFeatureList`)
  // and creates `LocationBarBadgeTabHelper` for `web_state_`, asserting that it
  // starts with no displayed badge request.
  LocationBarBadgeTabHelper* CreateLocationBarBadgeTabHelper() {
    web_state_.RemoveUserData(InfobarBadgeTabHelper::UserDataKey());
    InfobarBadgeTabHelper::CreateForWebState(&web_state_);
    delegate_.badgeTabHelper = tab_helper();
    tab_helper()->SetDelegate(delegate_);
    LocationBarBadgeTabHelper::CreateForWebState(&web_state_);
    LocationBarBadgeTabHelper* location_bar_badge_tab_helper =
        LocationBarBadgeTabHelper::FromWebState(&web_state_);
    CHECK(location_bar_badge_tab_helper);
    EXPECT_FALSE(
        location_bar_badge_tab_helper->GetDisplayedBadgeRequest().has_value());
    return location_bar_badge_tab_helper;
  }

  // Returns InfobarBadgeTabHelper attached to web_state_.
  InfobarBadgeTabHelper* tab_helper() {
    return InfobarBadgeTabHelper::FromWebState(&web_state_);
  }

  web::FakeWebState web_state_;
  TestInfobarTabHelperDelegate* delegate_ = nil;
};

// Test the badge state after changes to the state of an Infobar. Infobar badges
// should always be tappable.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeState) {
  // Add a badge-supporting infobar to the InfobarManager to create the badge
  // item.
  FakeInfobarIOS* infobar = AddInfobar(/*has_badge=*/true);
  InfobarType added_type = infobar->infobar_type();
  // Simulate presenting the banner UI and verify that the badge state is sent
  // to the delegate.
  tab_helper()->UpdateBadgeForInfobarBannerPresented(added_type);
  EXPECT_TRUE(tab_helper()->GetInfobarBadgeStates()[added_type] &
              BadgeStatePresented);
  id<BadgeItem> item = [delegate_ itemForInfobarType:added_type];
  EXPECT_TRUE(item);
  EXPECT_TRUE(item.tappable);
  EXPECT_TRUE(item.badgeState & BadgeStatePresented);
  // Simulate accepting the infobar and verify that the badge state is udpated.
  infobar->set_accepted(true);
  EXPECT_TRUE(tab_helper()->GetInfobarBadgeStates()[added_type] &
              BadgeStateAccepted);
  EXPECT_TRUE(tab_helper()->GetInfobarBadgeStates()[added_type] &
              BadgeStateRead);
  item = [delegate_ itemForInfobarType:added_type];
  EXPECT_TRUE(item.tappable);
  EXPECT_TRUE(item.badgeState & BadgeStateAccepted);
  EXPECT_TRUE(item.badgeState & BadgeStateRead);
  // Simulate reverting the infobar and verify that the badge state is udpated.
  infobar->set_accepted(false);
  EXPECT_FALSE(tab_helper()->GetInfobarBadgeStates()[added_type] &
               BadgeStateAccepted);
  item = [delegate_ itemForInfobarType:added_type];
  EXPECT_FALSE(item.badgeState & BadgeStateAccepted);
}

// Test the badge state after changes to the state of an Infobar. Infobar badges
// should always be tappable.  Uses deprecated API.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeStateDeprecated) {
  // Add a badge-supporting infobar to the InfobarManager to create the badge
  // item.
  InfobarType added_type = AddInfobar(/*has_badge=*/true)->infobar_type();
  // Simulate presenting the banner UI and verify that the badge state is sent
  // to the delegate.
  tab_helper()->UpdateBadgeForInfobarBannerPresented(added_type);
  EXPECT_TRUE(tab_helper()->GetInfobarBadgeStates()[added_type] &
              BadgeStatePresented);
  id<BadgeItem> item = [delegate_ itemForInfobarType:added_type];
  EXPECT_TRUE(item);
  EXPECT_TRUE(item.tappable);
  EXPECT_TRUE(item.badgeState & BadgeStatePresented);
  // Simulate accepting the infobar and verify that the badge state is udpated.
  tab_helper()->UpdateBadgeForInfobarAccepted(added_type);
  EXPECT_TRUE(tab_helper()->GetInfobarBadgeStates()[added_type] &
              BadgeStateAccepted);
  EXPECT_TRUE(tab_helper()->GetInfobarBadgeStates()[added_type] &
              BadgeStateRead);
  item = [delegate_ itemForInfobarType:added_type];
  EXPECT_TRUE(item.tappable);
  EXPECT_TRUE(item.badgeState & BadgeStateAccepted);
  EXPECT_TRUE(item.badgeState & BadgeStateRead);
  // Simulate reverting the infobar and verify that the badge state is udpated.
  tab_helper()->UpdateBadgeForInfobarReverted(added_type);
  EXPECT_FALSE(tab_helper()->GetInfobarBadgeStates()[added_type] &
               BadgeStateAccepted);
  item = [delegate_ itemForInfobarType:added_type];
  EXPECT_FALSE(item.badgeState & BadgeStateAccepted);
}

// Tests that adding an infobar that doesn't support badges does not notify the
// delegate of BadgeItem creation.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeStateNoBadge) {
  InfobarType added_type = AddInfobar(/*has_badge=*/false)->infobar_type();
  std::map<InfobarType, BadgeState> badge_states =
      tab_helper()->GetInfobarBadgeStates();
  EXPECT_FALSE(badge_states.contains(added_type));
  EXPECT_FALSE([delegate_ itemForInfobarType:added_type]);
}

// Tests that the InfobarBadge has not been removed after dismissing the
// InfobarBanner.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeOnBannerDismissal) {
  // Add a badge-supporting infobar to the InfobarManager to create the badge
  // item, then simulate presentation and dismissal.
  InfobarType added_type = AddInfobar(/*has_badge=*/true)->infobar_type();
  tab_helper()->UpdateBadgeForInfobarBannerPresented(added_type);
  tab_helper()->UpdateBadgeForInfobarBannerDismissed(added_type);
  // Verify that the BadgeItem was not removed and that its state is dismissed.
  EXPECT_FALSE(tab_helper()->GetInfobarBadgeStates()[added_type] &
               BadgeStatePresented);
  id<BadgeItem> item = [delegate_ itemForInfobarType:added_type];
  EXPECT_TRUE(item);
  EXPECT_FALSE(item.badgeState & BadgeStatePresented);
}

// Test that the Accepted badge state remains after dismissing the
// InfobarBanner.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeOnBannerAccepted) {
  // Add a badge-supporting infobar to the InfobarManager to create the badge
  // item, then simulate presentation, acceptance, and dismissal.
  FakeInfobarIOS* infobar = AddInfobar(/*has_badge=*/true);
  InfobarType added_type = infobar->infobar_type();
  tab_helper()->UpdateBadgeForInfobarBannerPresented(added_type);
  infobar->set_accepted(true);
  tab_helper()->UpdateBadgeForInfobarBannerDismissed(added_type);
  // Verify that the BadgeItem was not removed and that its state is dismissed.
  EXPECT_TRUE(tab_helper()->GetInfobarBadgeStates()[added_type] &
              BadgeStateAccepted);
  id<BadgeItem> item = [delegate_ itemForInfobarType:added_type];
  EXPECT_TRUE(item);
  EXPECT_TRUE(item.badgeState & BadgeStateAccepted);
}

// Test that the Accepted badge state remains after dismissing the
// InfobarBanner.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeOnBannerAcceptedDeprecated) {
  // Add a badge-supporting infobar to the InfobarManager to create the badge
  // item, then simulate presentation, acceptance, and dismissal.
  InfobarType added_type = AddInfobar(/*has_badge=*/true)->infobar_type();
  tab_helper()->UpdateBadgeForInfobarBannerPresented(added_type);
  tab_helper()->UpdateBadgeForInfobarAccepted(added_type);
  tab_helper()->UpdateBadgeForInfobarBannerDismissed(added_type);
  // Verify that the BadgeItem was not removed and that its state is dismissed.
  EXPECT_TRUE(tab_helper()->GetInfobarBadgeStates()[added_type] &
              BadgeStateAccepted);
  id<BadgeItem> item = [delegate_ itemForInfobarType:added_type];
  EXPECT_TRUE(item);
  EXPECT_TRUE(item.badgeState & BadgeStateAccepted);
}

// Test that destroying the InfobarView stops displaying the badge.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarBadgeOnInfobarDestruction) {
  // Add a badge-supporting infobar to the InfobarManager to create the badge
  // item, then simulate presentation and dismissal.
  FakeInfobarIOS* added_infobar = AddInfobar(/*has_badge=*/true);
  InfobarType added_type = added_infobar->infobar_type();
  tab_helper()->UpdateBadgeForInfobarBannerPresented(added_type);
  tab_helper()->UpdateBadgeForInfobarBannerDismissed(added_type);
  ASSERT_TRUE([delegate_ itemForInfobarType:added_type]);
  // Remove the infobar from the manager and verify that the BadgeItem is also
  // removed.
  InfoBarManagerImpl::FromWebState(&web_state_)->RemoveInfoBar(added_infobar);
  std::map<InfobarType, BadgeState> badge_states =
      tab_helper()->GetInfobarBadgeStates();
  EXPECT_FALSE(badge_states.contains(added_type));
  EXPECT_FALSE([delegate_ itemForInfobarType:added_type]);
}

// Test that replacing infobar, doesn't crash.
TEST_F(InfobarBadgeTabHelperTest, TestInfobarReplacing) {
  // Test tab helper by driving it through InfoBarManager.
  AddInfobar(/*has_badge=*/true);
  // Check first one added correctly.
  EXPECT_EQ(InfoBarManagerImpl::FromWebState(&web_state_)->infobars().size(),
            1u);
  // Replace with second one.
  FakeInfobarIOS* infobar2 =
      AddInfobar(/*has_badge=*/true, /*replace_existing=*/true);
  // Should be only one.
  EXPECT_EQ(InfoBarManagerImpl::FromWebState(&web_state_)->infobars().size(),
            1u);
  // If first one wasn't replaced this will fail.
  InfoBarManagerImpl::FromWebState(&web_state_)->RemoveInfoBar(infobar2);
  // Left with none.
  EXPECT_EQ(InfoBarManagerImpl::FromWebState(&web_state_)->infobars().size(),
            0u);
  // No crash.
}

// Tests that when `kLocationBarBadgeMigration` is enabled, adding, updating,
// reverting, and removing an infobar synchronizes `LocationBarBadgeTabHelper`,
// including prerendering and non-committed navigations.
TEST_F(InfobarBadgeTabHelperTest,
       RegistersBadgeWhenLocationBarBadgeMigrationEnabled) {
  base::test::ScopedFeatureList feature_list(kLocationBarBadgeMigration);
  LocationBarBadgeTabHelper* location_bar_badge_tab_helper =
      CreateLocationBarBadgeTabHelper();
  ASSERT_TRUE(location_bar_badge_tab_helper);

  // Add an unbadged infobar (e.g. confirm) and verify no badge is registered.
  FakeInfobarIOS* unbadged_infobar = AddInfobar(/*has_badge=*/false);
  EXPECT_FALSE(
      location_bar_badge_tab_helper->GetDisplayedBadgeRequest().has_value());
  InfoBarManagerImpl::FromWebState(&web_state_)
      ->RemoveInfoBar(unbadged_infobar);

  // Add an infobar while `delegate_` is `nil` (prerender case) and verify it
  // registers on `LocationBarBadgeTabHelper` and survives `SetDelegate`.
  tab_helper()->SetDelegate(nil);
  FakeInfobarIOS* infobar = AddInfobarWithType(kInfobarTypeWithMigratedBadge);
  tab_helper()->SetDelegate(delegate_);
  std::optional<LocationBarBadgeRequest> request =
      location_bar_badge_tab_helper->GetDisplayedBadgeRequest();
  ASSERT_TRUE(request.has_value());
  EXPECT_EQ(request->badge_type, LocationBarBadgeType::kTranslate);
  EXPECT_TRUE(request->has_unread_indicator);
  EXPECT_FALSE(request->accepted);
  EXPECT_FALSE(request->is_highlighted);

  // Banner presentation highlights the badge; banner dismissal clears
  // highlight.
  tab_helper()->UpdateBadgeForInfobarBannerPresented(
      kInfobarTypeWithMigratedBadge);
  request = location_bar_badge_tab_helper->GetDisplayedBadgeRequest();
  ASSERT_TRUE(request.has_value());
  EXPECT_TRUE(request->is_highlighted);

  tab_helper()->UpdateBadgeForInfobarBannerDismissed(
      kInfobarTypeWithMigratedBadge);
  request = location_bar_badge_tab_helper->GetDisplayedBadgeRequest();
  ASSERT_TRUE(request.has_value());
  EXPECT_FALSE(request->is_highlighted);

  // Reading the infobar clears the unread indicator.
  tab_helper()->UpdateBadgeForInfobarRead(kInfobarTypeWithMigratedBadge);
  request = location_bar_badge_tab_helper->GetDisplayedBadgeRequest();
  ASSERT_TRUE(request.has_value());
  EXPECT_FALSE(request->has_unread_indicator);

  // Accepting marks it accepted; reverting clears accepted.
  infobar->set_accepted(true);
  request = location_bar_badge_tab_helper->GetDisplayedBadgeRequest();
  ASSERT_TRUE(request.has_value());
  EXPECT_TRUE(request->accepted);

  infobar->set_accepted(false);
  request = location_bar_badge_tab_helper->GetDisplayedBadgeRequest();
  ASSERT_TRUE(request.has_value());
  EXPECT_FALSE(request->accepted);

  // Non-committed cross-document navigation re-syncs the surviving infobar.
  web::FakeNavigationContext navigation_context_uncommitted;
  navigation_context_uncommitted.SetIsSameDocument(false);
  navigation_context_uncommitted.SetHasCommitted(false);
  web_state_.OnNavigationStarted(&navigation_context_uncommitted);
  EXPECT_FALSE(
      location_bar_badge_tab_helper->GetDisplayedBadgeRequest().has_value());
  web_state_.OnNavigationFinished(&navigation_context_uncommitted);
  request = location_bar_badge_tab_helper->GetDisplayedBadgeRequest();
  ASSERT_TRUE(request.has_value());
  EXPECT_EQ(request->badge_type, LocationBarBadgeType::kTranslate);
  EXPECT_FALSE(request->accepted);

  // Committed cross-document navigation also re-syncs surviving infobars.
  web::FakeNavigationContext navigation_context_committed;
  navigation_context_committed.SetIsSameDocument(false);
  navigation_context_committed.SetHasCommitted(true);
  web_state_.OnNavigationStarted(&navigation_context_committed);
  EXPECT_FALSE(
      location_bar_badge_tab_helper->GetDisplayedBadgeRequest().has_value());
  web_state_.OnNavigationFinished(&navigation_context_committed);
  request = location_bar_badge_tab_helper->GetDisplayedBadgeRequest();
  ASSERT_TRUE(request.has_value());
  EXPECT_EQ(request->badge_type, LocationBarBadgeType::kTranslate);
  EXPECT_FALSE(request->accepted);

  InfoBarManagerImpl::FromWebState(&web_state_)->RemoveInfoBar(infobar);
  EXPECT_FALSE(
      location_bar_badge_tab_helper->GetDisplayedBadgeRequest().has_value());
}

// Tests that when `kLocationBarBadgeMigration` is disabled, adding an infobar
// does not register a badge on `LocationBarBadgeTabHelper`.
TEST_F(InfobarBadgeTabHelperTest,
       DoesNotRegisterBadgeWhenLocationBarBadgeMigrationDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kLocationBarBadgeMigration);

  LocationBarBadgeTabHelper* location_bar_badge_tab_helper =
      CreateLocationBarBadgeTabHelper();
  ASSERT_TRUE(location_bar_badge_tab_helper);

  FakeInfobarIOS* infobar = AddInfobar(/*has_badge=*/true);
  EXPECT_FALSE(
      location_bar_badge_tab_helper->GetDisplayedBadgeRequest().has_value());
  EXPECT_TRUE([delegate_ itemForInfobarType:kInfobarTypeWithBadge]);

  InfoBarManagerImpl::FromWebState(&web_state_)->RemoveInfoBar(infobar);
  EXPECT_FALSE(
      location_bar_badge_tab_helper->GetDisplayedBadgeRequest().has_value());
}
