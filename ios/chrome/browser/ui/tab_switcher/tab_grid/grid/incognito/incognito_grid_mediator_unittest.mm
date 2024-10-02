// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator.h"

#import "components/policy/core/common/policy_pref_names.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_mediator_test.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/test/fake_tab_grid_toolbars_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/test/fake_tab_collection_consumer.h"
#import "ios/web/public/web_state_id.h"

class IncognitoGridMediatorTest : public GridMediatorTestClass {
 public:
  IncognitoGridMediatorTest() {}
  ~IncognitoGridMediatorTest() override {}

  void SetUp() override {
    GridMediatorTestClass::SetUp();
    mode_holder_ = [[TabGridModeHolder alloc] init];
    mediator_ = [[IncognitoGridMediator alloc] initWithModeHolder:mode_holder_];
    mediator_.consumer = consumer_;
    mediator_.browser = browser_.get();
    mediator_.toolbarsMutator = fake_toolbars_mediator_;
    [mediator_ currentlySelectedGrid:YES];
  }

  void TearDown() override {
    // Forces the IncognitoGridMediator to removes its Observer from
    // WebStateList before the Browser is destroyed.
    [mediator_ disconnect];
    mediator_ = nil;
    GridMediatorTestClass::TearDown();
  }

 protected:
  IncognitoGridMediator* mediator_;
  TabGridModeHolder* mode_holder_;
};

// Tests that the WebStateList and consumer's list are empty when
// `-closeAllItems` is called.
TEST_F(IncognitoGridMediatorTest, CloseAllItemsCommand) {
  // Previously there were 3 items.
  [mediator_ closeAllItems];
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.size());
}

// Checks that opening a new regular tab from the toolbar is done when allowed.
TEST_F(IncognitoGridMediatorTest, OpenNewTab_OpenIfAllowedByPolicy) {
  // Disconnect the existing mediator first as we will re-create it.
  [mediator_ disconnect];

  // IncognitoModePrefs::kEnabled Means that users may open pages in both
  // Incognito mode and normal mode
  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kEnabled)));
  mediator_ = [[IncognitoGridMediator alloc] initWithModeHolder:mode_holder_];
  mediator_.consumer = consumer_;
  mediator_.browser = browser_.get();
  EXPECT_EQ(3, browser_->GetWebStateList()->count());

  // Emulate tapping one the new tab button by using the actions wrangler
  // interface that would normally be called by the tap action target.
  [mediator_ newTabButtonTapped:nil];

  EXPECT_EQ(4, browser_->GetWebStateList()->count())
      << "Can not open an incognito tab by calling new tab button function "
         "when policy is the default value.";

  // IncognitoModePrefs::kDisabled Means that users may not open pages in
  // Incognito mode. Only normal mode is available for browsing.
  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kDisabled)));

  // Disconnect the mediator as we will destroy it.
  [mediator_ disconnect];

  mediator_ = [[IncognitoGridMediator alloc] initWithModeHolder:mode_holder_];
  mediator_.consumer = consumer_;
  mediator_.browser = browser_.get();
  EXPECT_EQ(4, browser_->GetWebStateList()->count());
  [mediator_ newTabButtonTapped:nil];
  EXPECT_EQ(4, browser_->GetWebStateList()->count())
      << "Can open an incognito tab by calling new tab button function when "
         "policy should disable incognito.";
}

// Ensures that when there is *no* web states in normal mode, the toolbar
// configuration is correct.
TEST_F(IncognitoGridMediatorTest, TestToolbarsNormalModeWithoutWebstates) {
  EXPECT_EQ(3UL, consumer_.items.size());
  [mediator_ closeAllItems];
  EXPECT_EQ(0UL, consumer_.items.size());

  EXPECT_EQ(TabGridPageIncognitoTabs,
            fake_toolbars_mediator_.configuration.page);

  EXPECT_TRUE(fake_toolbars_mediator_.configuration.newTabButton);
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.searchButton);

  EXPECT_FALSE(fake_toolbars_mediator_.configuration.closeAllButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.doneButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.selectTabsButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.undoButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.deselectAllButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.selectAllButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.addToButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.shareButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.cancelSearchButton);
}
