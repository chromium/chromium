// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_mediator.h"

#import "base/containers/contains.h"
#import "base/memory/raw_ptr.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/sessions/core/tab_restore_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_mediator_test.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/test/fake_tab_grid_toolbars_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/test/fake_tab_collection_consumer.h"
#import "ios/web/public/test/fakes/fake_web_state.h"

class RegularGridMediatorTest : public GridMediatorTestClass {
 public:
  RegularGridMediatorTest() {}
  ~RegularGridMediatorTest() override {}

  void SetUp() override {
    GridMediatorTestClass::SetUp();
    mode_holder_ = [[TabGridModeHolder alloc] init];
    mediator_ = [[RegularGridMediator alloc] initWithModeHolder:mode_holder_];
    mediator_.consumer = consumer_;
    mediator_.browser = browser_.get();
    mediator_.toolbarsMutator = fake_toolbars_mediator_;
    [mediator_ currentlySelectedGrid:YES];

    tab_restore_service_ =
        IOSChromeTabRestoreServiceFactory::GetForProfile(profile_.get());
  }

  void TearDown() override {
    // Forces the RegularGridMediator to removes its Observer from WebStateList
    // before the Browser is destroyed.
    mediator_.browser = nullptr;
    mediator_ = nil;
    GridMediatorTestClass::TearDown();
  }

 protected:
  RegularGridMediator* mediator_ = nullptr;
  raw_ptr<sessions::TabRestoreService> tab_restore_service_ = nullptr;
  TabGridModeHolder* mode_holder_;
};

#pragma mark - Command tests

// Tests that the WebStateList and consumer's list are empty when
// `-saveAndCloseAllItems` is called.
TEST_F(RegularGridMediatorTest, SaveAndCloseAllItemsCommand) {
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.size());
}

// Tests that the WebStateList is not restored to 3 items when
// `-undoCloseAllItems` is called after `-discardSavedClosedItems` is called.
TEST_F(RegularGridMediatorTest, DiscardSavedClosedItemsCommand) {
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  [mediator_ discardSavedClosedItems];
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.size());
}

// Tests that the WebStateList is restored to 3 items when
// `-undoCloseAllItems` is called.
TEST_F(RegularGridMediatorTest, UndoCloseAllItemsCommand) {
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(3, browser_->GetWebStateList()->count());
  EXPECT_EQ(3UL, consumer_.items.size());
  EXPECT_TRUE(base::Contains(original_identifiers_, consumer_.items[0]));
  EXPECT_TRUE(base::Contains(original_identifiers_, consumer_.items[1]));
  EXPECT_TRUE(base::Contains(original_identifiers_, consumer_.items[2]));
}

// Tests that the WebStateList is restored to 3 items when
// `-undoCloseAllItems` is called.
TEST_F(RegularGridMediatorTest, UndoCloseAllItemsCommandWithNTP) {
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];

  // There should be no tabs in the WebStateList.
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.size());

  // There should be no "recently closed items" yet.
  EXPECT_EQ(0u, tab_restore_service_->entries().size());

  // Discarding the saved item should add them to recently closed.
  [mediator_ discardSavedClosedItems];
  EXPECT_EQ(3u, tab_restore_service_->entries().size());

  // Add three new tabs.
  auto web_state1 = CreateFakeWebStateWithURL(GURL("https://test/url1"));
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state1), WebStateList::InsertionParams::AtIndex(0));
  // Second tab is a NTP.
  auto web_state2 = CreateFakeWebStateWithURL(GURL(kChromeUINewTabURL));
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state2), WebStateList::InsertionParams::AtIndex(1));
  auto web_state3 = CreateFakeWebStateWithURL(GURL("https://test/url2"));
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state3), WebStateList::InsertionParams::AtIndex(2));
  browser_->GetWebStateList()->ActivateWebStateAt(0);

  // Closing item does not add them to the recently closed.
  [mediator_ saveAndCloseAllItems];

  // There should be no tabs in the WebStateList.
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.size());

  // There should be no new "recently closed items".
  EXPECT_EQ(3u, tab_restore_service_->entries().size());

  // Undoing the close should restore the items.
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(3UL, consumer_.items.size());
}

// Checks that opening a new regular tab from the toolbar is done when allowed.
TEST_F(RegularGridMediatorTest, OpenNewTab_OpenIfAllowedByPolicy) {
  // IncognitoModePrefs::kEnabled Means that users may open pages in both
  // Incognito mode and normal mode
  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kEnabled)));
  EXPECT_EQ(3, browser_->GetWebStateList()->count());

  // Emulate tapping one the new tab button by using the actions wrangler
  // interface that would normally be called by the tap action target.
  [mediator_ newTabButtonTapped:nil];

  EXPECT_EQ(4, browser_->GetWebStateList()->count())
      << "Can not open a regular tab by calling new tab button function when "
         "policy is the default value.";

  // IncognitoModePrefs::kDisabled Means that users may not open pages in
  // Incognito mode. Only normal mode is available for browsing.
  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kDisabled)));

  EXPECT_EQ(4, browser_->GetWebStateList()->count());
  [mediator_ newTabButtonTapped:nil];
  EXPECT_EQ(5, browser_->GetWebStateList()->count())
      << "Can not open a regular tab by calling new tab button function when "
         "policy should disable incognito.";

  // IncognitoModePrefs::kForced Means that users may open pages *ONLY* in
  // Incognito mode. Normal mode is not available for browsing.
  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kForced)));
  EXPECT_EQ(5, browser_->GetWebStateList()->count());
  [mediator_ newTabButtonTapped:nil];
  EXPECT_EQ(5, browser_->GetWebStateList()->count())
      << "Can open a regular tab by calling new tab button function when "
         "policy force incognito only.";
}

// Ensures that when there is *no* web states in normal mode, the toolbar
// configuration is correct.
TEST_F(RegularGridMediatorTest, TestToolbarsNormalModeWithoutWebstates) {
  EXPECT_EQ(3UL, consumer_.items.size());
  [mediator_ saveAndCloseAllItems];
  EXPECT_EQ(0UL, consumer_.items.size());

  EXPECT_EQ(TabGridPageRegularTabs, fake_toolbars_mediator_.configuration.page);

  EXPECT_TRUE(fake_toolbars_mediator_.configuration.newTabButton);
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.searchButton);
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.undoButton);

  EXPECT_FALSE(fake_toolbars_mediator_.configuration.closeAllButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.doneButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.selectTabsButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.deselectAllButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.selectAllButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.addToButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.shareButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.cancelSearchButton);
}
