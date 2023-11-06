// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_mediator.h"

#import "base/containers/contains.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/sessions/core/tab_restore_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_mediator_test.h"
#import "ios/chrome/browser/ui/tab_switcher/test/fake_tab_collection_consumer.h"
#import "ios/web/public/test/fakes/fake_web_state.h"

class RegularGridMediatorTest : public GridMediatorTestClass {
 public:
  RegularGridMediatorTest() {}
  ~RegularGridMediatorTest() override {}

  void SetUp() override {
    GridMediatorTestClass::SetUp();
    mediator_ = [[RegularGridMediator alloc] init];
    mediator_.consumer = consumer_;
    mediator_.browser = browser_.get();

    tab_restore_service_ =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(
            browser_state_.get());
    mediator_.tabRestoreService = tab_restore_service_;
  }

  void TearDown() override {
    // Forces the RegularGridMediator to removes its Observer from WebStateList
    // before the Browser is destroyed.
    mediator_.browser = nullptr;
    mediator_ = nil;
    GridMediatorTestClass::TearDown();
  }

  // Prepare the mock method to restore the tabs.
  void PrepareForRestoration() {
    TestSessionService* test_session_service =
        [[TestSessionService alloc] init];
    SessionRestorationBrowserAgent::CreateForBrowser(
        browser_.get(), test_session_service, false);
    SessionRestorationBrowserAgent::FromBrowser(browser_.get())
        ->SetSessionID([[NSUUID UUID] UUIDString]);
  }

 protected:
  RegularGridMediator* mediator_;
  sessions::TabRestoreService* tab_restore_service_;
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
  PrepareForRestoration();
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
  PrepareForRestoration();
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
  PrepareForRestoration();
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  // The three tabs created in the SetUp should be passed to the restore
  // service.
  EXPECT_EQ(3UL, tab_restore_service_->entries().size());
  std::set<SessionID::id_type> ids;
  for (auto& entry : tab_restore_service_->entries()) {
    ids.insert(entry->id.id());
  }
  EXPECT_EQ(3UL, ids.size());
  // There should be no tabs in the WebStateList.
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.size());

  // Add three new tabs.
  auto web_state1 = CreateFakeWebStateWithURL(GURL("https://test/url1"));
  browser_->GetWebStateList()->InsertWebState(0, std::move(web_state1),
                                              WebStateList::INSERT_FORCE_INDEX,
                                              WebStateOpener());
  // Second tab is a NTP.
  auto web_state2 = CreateFakeWebStateWithURL(GURL(kChromeUINewTabURL));
  browser_->GetWebStateList()->InsertWebState(1, std::move(web_state2),
                                              WebStateList::INSERT_FORCE_INDEX,
                                              WebStateOpener());
  auto web_state3 = CreateFakeWebStateWithURL(GURL("https://test/url2"));
  browser_->GetWebStateList()->InsertWebState(2, std::move(web_state3),
                                              WebStateList::INSERT_FORCE_INDEX,
                                              WebStateOpener());
  browser_->GetWebStateList()->ActivateWebStateAt(0);

  [mediator_ saveAndCloseAllItems];
  // The NTP should not be saved.
  EXPECT_EQ(5UL, tab_restore_service_->entries().size());
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.size());
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(3UL, tab_restore_service_->entries().size());
  EXPECT_EQ(3UL, consumer_.items.size());
  // Check the session entries were not changed.
  for (auto& entry : tab_restore_service_->entries()) {
    EXPECT_EQ(1UL, ids.count(entry->id.id()));
  }
}

// Checks that opening a new regular tab from the toolbar is done when allowed.
TEST_F(RegularGridMediatorTest, OpenNewTab_OpenIfAllowedByPolicy) {
  // IncognitoModePrefs::kEnabled Means that users may open pages in both
  // Incognito mode and normal mode
  browser_state_->GetTestingPrefService()->SetManagedPref(
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
  browser_state_->GetTestingPrefService()->SetManagedPref(
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
  browser_state_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kForced)));
  EXPECT_EQ(5, browser_->GetWebStateList()->count());
  [mediator_ newTabButtonTapped:nil];
  EXPECT_EQ(5, browser_->GetWebStateList()->count())
      << "Can open a regular tab by calling new tab button function when "
         "policy force incognito only.";
}
