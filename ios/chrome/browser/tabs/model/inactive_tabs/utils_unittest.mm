// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/inactive_tabs/utils.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/utils.h"
#import "ios/chrome/browser/web/web_navigation_util.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/device_form_factor.h"

// Fake WebStateList delegate that attaches the required tab helper.
class InactiveTabsFakeWebStateListDelegate : public FakeWebStateListDelegate {
 public:
  InactiveTabsFakeWebStateListDelegate() {}
  ~InactiveTabsFakeWebStateListDelegate() override {}

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override {
    SnapshotTabHelper::CreateForWebState(web_state);
  }
};

class InactiveTabsUtilsTest : public PlatformTest {
 public:
  InactiveTabsUtilsTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_active_ = std::make_unique<TestBrowser>(
        browser_state_.get(),
        std::make_unique<InactiveTabsFakeWebStateListDelegate>());
    browser_inactive_ = std::make_unique<TestBrowser>(
        browser_state_.get(),
        std::make_unique<InactiveTabsFakeWebStateListDelegate>());
    SnapshotBrowserAgent::CreateForBrowser(browser_active_.get());
    SnapshotBrowserAgent::CreateForBrowser(browser_inactive_.get());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_active_;
  std::unique_ptr<TestBrowser> browser_inactive_;
  // Used to verify histogram logging.
  base::HistogramTester histogram_tester_;

  std::unique_ptr<web::FakeWebState> CreateTab(
      web::WebStateID unique_identifier,
      base::Time last_active_time) {
    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>(unique_identifier);
    web_state->SetLastActiveTime(last_active_time);
    return web_state;
  }

  std::unique_ptr<web::FakeWebState> CreateActiveTab() {
    return CreateTab(web::WebStateID::NewUnique(), base::Time::Now());
  }

  std::unique_ptr<web::FakeWebState> CreateInactiveTab(base::TimeDelta delta) {
    return CreateTab(web::WebStateID::NewUnique(), base::Time::Now() - delta);
  }

  void CheckOrder(WebStateList* web_state_list,
                  std::vector<int> expected_inactivity_days) {
    for (int index = 0; index < web_state_list->count(); index++) {
      web::WebState* current_web_state = web_state_list->GetWebStateAt(index);
      int time_since_last_activation =
          (base::Time::Now() - current_web_state->GetLastActiveTime()).InDays();
      ASSERT_LT(index, static_cast<int>(expected_inactivity_days.size()));
      EXPECT_EQ(time_since_last_activation, expected_inactivity_days[index]);
    }
  }
};

// Ensure that the active tab in the active tab list with date set at "Now" is
// not added to the inactive tab list.
TEST_F(InactiveTabsUtilsTest, ActiveTabStaysActive) {
  // No inactive tabs on iPad.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters[kTabInactivityThresholdParameterName] =
      kTabInactivityThresholdOneWeekParam;
  feature_list.InitAndEnableFeatureWithParameters(kTabInactivityThreshold,
                                                  parameters);

  WebStateList* active_web_state_list = browser_active_->GetWebStateList();
  WebStateList* inactive_web_state_list = browser_inactive_->GetWebStateList();

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  // Add a new active tab in the active browser.
  active_web_state_list->InsertWebState(
      0, CreateActiveTab(), WebStateList::INSERT_ACTIVATE, WebStateOpener());

  EXPECT_EQ(active_web_state_list->count(), 1);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  MoveTabsFromActiveToInactive(browser_active_.get(), browser_inactive_.get());

  EXPECT_EQ(active_web_state_list->count(), 1);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  // Expect a log of 0 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnMigrateActiveToInactive", 0, 1);
}

// Ensure that inactive tabs are moved from the active tab list to the inactive
// tab list.
TEST_F(InactiveTabsUtilsTest, InactiveTabAreMovedFromActiveList) {
  // No inactive tabs on iPad.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters[kTabInactivityThresholdParameterName] =
      kTabInactivityThresholdOneWeekParam;
  feature_list.InitAndEnableFeatureWithParameters(kTabInactivityThreshold,
                                                  parameters);

  WebStateList* active_web_state_list = browser_active_->GetWebStateList();
  WebStateList* inactive_web_state_list = browser_inactive_->GetWebStateList();

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  // Add a new inactive tab (10 days with no activity) in the active browser.
  active_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(10)),
                                        WebStateList::INSERT_ACTIVATE,
                                        WebStateOpener());

  EXPECT_EQ(active_web_state_list->count(), 1);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  MoveTabsFromActiveToInactive(browser_active_.get(), browser_inactive_.get());

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 1);

  // Expect a log of 0 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnMigrateActiveToInactive", 0, 1);
}

// Ensure there is no active tab in the inactive tab list.
TEST_F(InactiveTabsUtilsTest, ActiveTabAreMovedFromInactiveList) {
  // No inactive tabs on iPad.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters[kTabInactivityThresholdParameterName] =
      kTabInactivityThresholdOneWeekParam;
  feature_list.InitAndEnableFeatureWithParameters(kTabInactivityThreshold,
                                                  parameters);

  WebStateList* active_web_state_list = browser_active_->GetWebStateList();
  WebStateList* inactive_web_state_list = browser_inactive_->GetWebStateList();

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  // Add a new active tab in the inactive browser.
  inactive_web_state_list->InsertWebState(
      0, CreateActiveTab(), WebStateList::INSERT_ACTIVATE, WebStateOpener());

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 1);

  MoveTabsFromInactiveToActive(browser_inactive_.get(), browser_active_.get());

  EXPECT_EQ(active_web_state_list->count(), 1);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  // Expect a log of 0 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnMigrateInactiveToActive", 0, 1);
}

// Ensure that inactive tab stay in inactive list.
TEST_F(InactiveTabsUtilsTest, InactiveTabStaysInactive) {
  // No inactive tabs on iPad.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters[kTabInactivityThresholdParameterName] =
      kTabInactivityThresholdOneWeekParam;
  feature_list.InitAndEnableFeatureWithParameters(kTabInactivityThreshold,
                                                  parameters);

  WebStateList* active_web_state_list = browser_active_->GetWebStateList();
  WebStateList* inactive_web_state_list = browser_inactive_->GetWebStateList();

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  // Add a new inactive tab (10 days without activity) in the inactive browser.
  inactive_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(10)),
                                          WebStateList::INSERT_ACTIVATE,
                                          WebStateOpener());

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 1);

  MoveTabsFromInactiveToActive(browser_inactive_.get(), browser_active_.get());

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 1);

  // Expect a log of 0 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnMigrateInactiveToActive", 0, 1);
}

// Restore all inactive tab.
TEST_F(InactiveTabsUtilsTest, RestoreAllInactive) {
  // RestoreAllInactive checks that it is called when the feature is disabled,
  // either via the flag, or via the user pref. Disable in both places.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kTabInactivityThreshold);
  local_state_.Get()->SetInteger(prefs::kInactiveTabsTimeThreshold, -1);

  WebStateList* active_web_state_list = browser_active_->GetWebStateList();
  WebStateList* inactive_web_state_list = browser_inactive_->GetWebStateList();

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  // Add a new inactive tab (10 days without activity) in the inactive browser.
  inactive_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(10)),
                                          WebStateList::INSERT_ACTIVATE,
                                          WebStateOpener());

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 1);

  RestoreAllInactiveTabs(browser_inactive_.get(), browser_active_.get());

  EXPECT_EQ(active_web_state_list->count(), 1);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  // Expect a log of 0 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnRestoreAllInactive", 0, 1);
}

// Ensure that all moving functions are working with complicated lists (multiple
// tabs, un-ordered, pinned tabs).
TEST_F(InactiveTabsUtilsTest, ComplicatedMove) {
  // No inactive tabs on iPad.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters[kTabInactivityThresholdParameterName] =
      kTabInactivityThresholdOneWeekParam;
  feature_list.InitWithFeaturesAndParameters(
      {/* Enabled features */
       {kTabInactivityThreshold, {parameters}}},
      {/* Disabled features */});

  WebStateList* active_web_state_list = browser_active_->GetWebStateList();
  WebStateList* inactive_web_state_list = browser_inactive_->GetWebStateList();

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  // Add a new inactive and active tabs in the inactive browser.
  inactive_web_state_list->InsertWebState(
      0, CreateActiveTab(), WebStateList::INSERT_ACTIVATE, WebStateOpener());
  inactive_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(10)),
                                          WebStateList::INSERT_ACTIVATE,
                                          WebStateOpener());
  inactive_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(30)),
                                          WebStateList::INSERT_ACTIVATE,
                                          WebStateOpener());
  inactive_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(2)),
                                          WebStateList::INSERT_ACTIVATE,
                                          WebStateOpener());
  inactive_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(16)),
                                          WebStateList::INSERT_ACTIVATE,
                                          WebStateOpener());
  inactive_web_state_list->InsertWebState(
      0, CreateActiveTab(), WebStateList::INSERT_ACTIVATE, WebStateOpener());

  // Add a new inactive and active tabs in the active browser.
  active_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(22)),
                                        WebStateList::INSERT_ACTIVATE,
                                        WebStateOpener());
  active_web_state_list->InsertWebState(
      0, CreateActiveTab(), WebStateList::INSERT_ACTIVATE, WebStateOpener());
  active_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(9)),
                                        WebStateList::INSERT_ACTIVATE,
                                        WebStateOpener());
  active_web_state_list->InsertWebState(
      0, CreateActiveTab(), WebStateList::INSERT_ACTIVATE, WebStateOpener());
  active_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(18)),
                                        WebStateList::INSERT_PINNED,
                                        WebStateOpener());
  active_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(3)),
                                        WebStateList::INSERT_ACTIVATE,
                                        WebStateOpener());

  EXPECT_EQ(active_web_state_list->count(), 6);
  EXPECT_EQ(inactive_web_state_list->count(), 6);

  // Pinned first (18) and then creation order (22, 0, 9, 0, 3).
  std::vector<int> expected_active_last_activity_order_before = {18, 22, 0,
                                                                 9,  0,  3};
  CheckOrder(active_web_state_list, expected_active_last_activity_order_before);
  // Creation order.
  std::vector<int> expected_inactive_last_activity_order_before = {0, 10, 30,
                                                                   2, 16, 0};
  CheckOrder(inactive_web_state_list,
             expected_inactive_last_activity_order_before);

  MoveTabsFromActiveToInactive(browser_active_.get(), browser_inactive_.get());

  EXPECT_EQ(active_web_state_list->count(), 4);
  EXPECT_EQ(inactive_web_state_list->count(), 8);

  // Expect a log of 0 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnMigrateActiveToInactive", 0, 1);

  // "old" inactive first (0, 10, 30, 2, 16, 0) and finally "new" inactive from
  // active list (22, 9).
  std::vector<int> expected_inactive_last_activity_order1 = {0,  10, 30, 2,
                                                             16, 0,  22, 9};
  CheckOrder(inactive_web_state_list, expected_inactive_last_activity_order1);

  // Pinned first (18) and then active (0, 0, 3).
  std::vector<int> expected_active_last_activity_order1 = {18, 0, 0, 3};
  CheckOrder(active_web_state_list, expected_active_last_activity_order1);

  MoveTabsFromInactiveToActive(browser_inactive_.get(), browser_active_.get());

  EXPECT_EQ(active_web_state_list->count(), 7);
  EXPECT_EQ(inactive_web_state_list->count(), 5);

  // Expect a log of 0 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnMigrateInactiveToActive", 0, 1);

  // All active (< 8 days of inactivity) are removed.
  std::vector<int> expected_inactive_last_activity_order2 = {10, 30, 16, 22, 9};
  CheckOrder(inactive_web_state_list, expected_inactive_last_activity_order2);

  // Pinned first (18) then "new" active from the inactive (0, 2, 0) then "old"
  // active (0, 0, 3).
  std::vector<int> expected_active_last_activity_order2 = {18, 0, 2, 0,
                                                           0,  0, 3};
  CheckOrder(active_web_state_list, expected_active_last_activity_order2);
}

// Ensure that restore function is working with complicated lists (multiple
// tabs, un-ordered, pinned tabs).
TEST_F(InactiveTabsUtilsTest, ComplicatedRestore) {
  // RestoreAllInactive checks that it is called when the feature is disabled,
  // either via the flag, or via the user pref. Disable in both places.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kTabInactivityThreshold);
  local_state_.Get()->SetInteger(prefs::kInactiveTabsTimeThreshold, -1);

  WebStateList* active_web_state_list = browser_active_->GetWebStateList();
  WebStateList* inactive_web_state_list = browser_inactive_->GetWebStateList();

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  // Add a new inactive and active tabs in the inactive browser.
  inactive_web_state_list->InsertWebState(
      0, CreateActiveTab(), WebStateList::INSERT_ACTIVATE, WebStateOpener());
  inactive_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(10)),
                                          WebStateList::INSERT_ACTIVATE,
                                          WebStateOpener());
  inactive_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(30)),
                                          WebStateList::INSERT_ACTIVATE,
                                          WebStateOpener());
  inactive_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(2)),
                                          WebStateList::INSERT_ACTIVATE,
                                          WebStateOpener());
  inactive_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(16)),
                                          WebStateList::INSERT_ACTIVATE,
                                          WebStateOpener());
  inactive_web_state_list->InsertWebState(
      0, CreateActiveTab(), WebStateList::INSERT_ACTIVATE, WebStateOpener());

  // Add pinned and active tab in the active browser.
  active_web_state_list->InsertWebState(
      0, CreateActiveTab(), WebStateList::INSERT_ACTIVATE, WebStateOpener());
  active_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(18)),
                                        WebStateList::INSERT_PINNED,
                                        WebStateOpener());

  EXPECT_EQ(active_web_state_list->count(), 2);
  EXPECT_EQ(inactive_web_state_list->count(), 6);

  RestoreAllInactiveTabs(browser_inactive_.get(), browser_active_.get());

  EXPECT_EQ(active_web_state_list->count(), 8);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  // Pinned first (18) then inactive (0, 10, 30, 2, 16, 0) and finally active
  // (0).
  std::vector<int> expected_last_activity_order = {18, 0, 10, 30, 2, 16, 0, 0};
  CheckOrder(active_web_state_list, expected_last_activity_order);

  // Expect a log of 0 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnRestoreAllInactive", 0, 1);
}

TEST_F(InactiveTabsUtilsTest, DoNotMoveNTPInInactive) {
  // No inactive tabs on iPad.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters[kTabInactivityThresholdParameterName] =
      kTabInactivityThresholdOneWeekParam;
  feature_list.InitAndEnableFeatureWithParameters(kTabInactivityThreshold,
                                                  parameters);

  // Needed to use the NewTabPageTabHelper and ensure that the tab is an NTP.
  std::unique_ptr<web::FakeNavigationManager> fake_navigation_manager =
      std::make_unique<web::FakeNavigationManager>();

  std::unique_ptr<web::NavigationItem> pending_item =
      web::NavigationItem::Create();
  pending_item->SetURL(GURL(kChromeUIAboutNewTabURL));
  fake_navigation_manager->SetPendingItem(pending_item.get());

  // Create a New Tab Page (NTP) tab with the last activity at 30 days ago.
  std::unique_ptr<web::FakeWebState> fake_web_state =
      std::make_unique<web::FakeWebState>();
  GURL url(kChromeUINewTabURL);
  fake_web_state->SetVisibleURL(url);
  fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));
  fake_web_state->SetLastActiveTime(base::Time::Now() - base::Days(30));

  // Ensure this is an ntp web state.
  id delegate = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));
  NewTabPageTabHelper::CreateForWebState(fake_web_state.get());
  NewTabPageTabHelper* ntp_helper =
      NewTabPageTabHelper::FromWebState(fake_web_state.get());
  ntp_helper->SetDelegate(delegate);
  ASSERT_TRUE(ntp_helper->IsActive());

  WebStateList* active_web_state_list = browser_active_->GetWebStateList();
  WebStateList* inactive_web_state_list = browser_inactive_->GetWebStateList();

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  // Add the created ntp in the active browser.
  active_web_state_list->InsertWebState(0, std::move(fake_web_state),
                                        WebStateList::INSERT_ACTIVATE,
                                        WebStateOpener());

  EXPECT_EQ(active_web_state_list->count(), 1);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  MoveTabsFromActiveToInactive(browser_active_.get(), browser_inactive_.get());

  EXPECT_EQ(active_web_state_list->count(), 1);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  // Expect a log of 0 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnMigrateActiveToInactive", 0, 1);
}

TEST_F(InactiveTabsUtilsTest, EnsurePreferencePriority) {
  // No inactive tabs on iPad.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters[kTabInactivityThresholdParameterName] =
      kTabInactivityThresholdOneWeekParam;
  feature_list.InitAndEnableFeatureWithParameters(kTabInactivityThreshold,
                                                  parameters);

  // Test that flags are taken into account instead of pref as we set the
  // preference default value.
  local_state_.Get()->SetInteger(prefs::kInactiveTabsTimeThreshold, 0);

  WebStateList* active_web_state_list = browser_active_->GetWebStateList();
  WebStateList* inactive_web_state_list = browser_inactive_->GetWebStateList();

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  // Add tabs in the active browser.
  active_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(3)),
                                        WebStateList::INSERT_ACTIVATE,
                                        WebStateOpener());
  active_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(10)),
                                        WebStateList::INSERT_ACTIVATE,
                                        WebStateOpener());
  active_web_state_list->InsertWebState(0, CreateInactiveTab(base::Days(30)),
                                        WebStateList::INSERT_ACTIVATE,
                                        WebStateOpener());

  EXPECT_EQ(active_web_state_list->count(), 3);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  MoveTabsFromActiveToInactive(browser_active_.get(), browser_inactive_.get());

  EXPECT_EQ(active_web_state_list->count(), 1);
  EXPECT_EQ(inactive_web_state_list->count(), 2);

  // Expect a log of 0 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnMigrateActiveToInactive", 0, 1);

  std::vector<int> expected_inactive_order = {10, 30};
  CheckOrder(inactive_web_state_list, expected_inactive_order);

  // Set the preference to 14.
  local_state_.Get()->SetInteger(prefs::kInactiveTabsTimeThreshold, 14);
  MoveTabsFromInactiveToActive(browser_inactive_.get(), browser_active_.get());

  // Expect a log of 0 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnMigrateInactiveToActive", 0, 1);

  EXPECT_EQ(active_web_state_list->count(), 2);
  EXPECT_EQ(inactive_web_state_list->count(), 1);

  std::vector<int> expected_active_order = {10, 3};
  CheckOrder(active_web_state_list, expected_active_order);
}

// Checks that Inactive Tabs migration method RestoreAllInactiveTabs filters out
// duplicates across browsers.
TEST_F(InactiveTabsUtilsTest, RestoreAllInactiveTabsRemovesCrossDuplicates) {
  // RestoreAllInactive checks that it is called when the feature is disabled,
  // either via the flag, or via the user pref. Disable in both places.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kTabInactivityThreshold);
  local_state_.Get()->SetInteger(prefs::kInactiveTabsTimeThreshold, -1);

  // Create known identifiers and last_active_time.
  const web::WebStateID unique_identifier = web::WebStateID::NewUnique();
  const base::Time last_active_time = base::Time::Now();

  // Create and insert an active tab with known identifiers.
  browser_active_->GetWebStateList()->InsertWebState(
      0, CreateTab(unique_identifier, last_active_time),
      WebStateList::INSERT_ACTIVATE, WebStateOpener());

  // Create and insert an inactive tab with the same identifiers.
  browser_inactive_->GetWebStateList()->InsertWebState(
      0, CreateTab(unique_identifier, last_active_time),
      WebStateList::INSERT_ACTIVATE, WebStateOpener());

  // Migrate back all inactive tabs to the active browser.
  RestoreAllInactiveTabs(browser_inactive_.get(), browser_active_.get());

  EXPECT_EQ(browser_active_->GetWebStateList()->count(), 1);
  EXPECT_EQ(browser_inactive_->GetWebStateList()->count(), 0);

  // Expect a log of 1 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnRestoreAllInactive", 1, 1);
}

// Checks that Inactive Tabs migration method MoveTabsFromInactiveToActive
// filters out duplicates across browsers.
TEST_F(InactiveTabsUtilsTest,
       MoveTabsFromInactiveToActiveRemovesCrossDuplicates) {
  // No inactive tabs on iPad.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters[kTabInactivityThresholdParameterName] =
      kTabInactivityThresholdOneWeekParam;
  feature_list.InitAndEnableFeatureWithParameters(kTabInactivityThreshold,
                                                  parameters);

  // Create known identifiers and last_active_time.
  const web::WebStateID unique_identifier = web::WebStateID::NewUnique();
  const base::Time last_active_time = base::Time::Now();

  // Create and insert an active tab with known identifiers.
  browser_active_->GetWebStateList()->InsertWebState(
      0, CreateTab(unique_identifier, last_active_time),
      WebStateList::INSERT_ACTIVATE, WebStateOpener());

  // Create and insert an inactive tab with the same identifiers.
  browser_inactive_->GetWebStateList()->InsertWebState(
      0, CreateTab(unique_identifier, last_active_time),
      WebStateList::INSERT_ACTIVATE, WebStateOpener());

  // Migrate back all inactive tabs to the active browser.
  MoveTabsFromInactiveToActive(browser_inactive_.get(), browser_active_.get());

  EXPECT_EQ(browser_active_->GetWebStateList()->count(), 1);
  EXPECT_EQ(browser_inactive_->GetWebStateList()->count(), 0);

  // Expect a log of 1 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnMigrateInactiveToActive", 1, 1);
}

// Checks that Inactive Tabs migration method MoveTabsFromActiveToInactive
// filters out duplicates across browsers.
TEST_F(InactiveTabsUtilsTest,
       MoveTabsFromActiveToInactiveRemovesCrossDuplicates) {
  // No inactive tabs on iPad.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    return;
  }
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> parameters;
  parameters[kTabInactivityThresholdParameterName] =
      kTabInactivityThresholdOneWeekParam;
  feature_list.InitAndEnableFeatureWithParameters(kTabInactivityThreshold,
                                                  parameters);

  // Create known identifiers and last_active_time.
  const web::WebStateID unique_identifier = web::WebStateID::NewUnique();
  const base::Time last_active_time = base::Time::Now() - base::Days(10);

  // Create and insert an active tab with known identifiers.
  browser_active_->GetWebStateList()->InsertWebState(
      0, CreateTab(unique_identifier, last_active_time),
      WebStateList::INSERT_ACTIVATE, WebStateOpener());

  // Create and insert an inactive tab with the same identifiers.
  browser_inactive_->GetWebStateList()->InsertWebState(
      0, CreateTab(unique_identifier, last_active_time),
      WebStateList::INSERT_ACTIVATE, WebStateOpener());

  // Migrate back all inactive tabs to the active browser.
  MoveTabsFromActiveToInactive(browser_active_.get(), browser_inactive_.get());

  EXPECT_EQ(browser_active_->GetWebStateList()->count(), 0);
  EXPECT_EQ(browser_inactive_->GetWebStateList()->count(), 1);

  // Expect a log of 1 duplicate.
  histogram_tester_.ExpectUniqueSample(
      "Tabs.DroppedDuplicatesCountOnMigrateActiveToInactive", 1, 1);
}
