// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator.h"

#import "components/policy/core/common/policy_pref_names.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_mediator_test.h"
#import "ios/chrome/browser/ui/tab_switcher/test/fake_tab_collection_consumer.h"
#import "ios/web/public/web_state_id.h"

class IncognitoGridMediatorTest : public GridMediatorTestClass {
 public:
  IncognitoGridMediatorTest() {}
  ~IncognitoGridMediatorTest() override {}

  void SetUp() override {
    GridMediatorTestClass::SetUp();
    mediator_ = [[IncognitoGridMediator alloc] init];
    mediator_.consumer = consumer_;
    mediator_.browser = browser_.get();
  }

  void TearDown() override {
    // Forces the IncognitoGridMediator to removes its Observer from
    // WebStateList before the Browser is destroyed.
    mediator_.browser = nullptr;
    mediator_ = nil;
    GridMediatorTestClass::TearDown();
  }

 protected:
  IncognitoGridMediator* mediator_;
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
  // IncognitoModePrefs::kEnabled Means that users may open pages in both
  // Incognito mode and normal mode
  browser_state_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kEnabled)));
  mediator_ = [[IncognitoGridMediator alloc] init];
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
  browser_state_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kDisabled)));
  mediator_ = [[IncognitoGridMediator alloc] init];
  mediator_.consumer = consumer_;
  mediator_.browser = browser_.get();
  EXPECT_EQ(4, browser_->GetWebStateList()->count());
  [mediator_ newTabButtonTapped:nil];
  EXPECT_EQ(4, browser_->GetWebStateList()->count())
      << "Can open an incognito tab by calling new tab button function when "
         "policy should disable incognito.";
}
