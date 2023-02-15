// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/inactive_tabs/utils.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/tabs/inactive_tabs/utils.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class InactiveTabsUtilsTest : public PlatformTest {
 public:
  InactiveTabsUtilsTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_active_ = std::make_unique<TestBrowser>(browser_state_.get());
    browser_inactive_ = std::make_unique<TestBrowser>(browser_state_.get());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_active_;
  std::unique_ptr<TestBrowser> browser_inactive_;

  std::unique_ptr<web::FakeWebState> CreateActiveTab() {
    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    web_state->SetLastActiveTime(base::Time::Now());
    return web_state;
  }

  std::unique_ptr<web::FakeWebState> CreateInactiveTab(
      int inactivity_days_number) {
    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    web_state->SetLastActiveTime(base::Time::Now() -
                                 base::Days(inactivity_days_number));
    return web_state;
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
  active_web_state_list->InsertWebState(0, CreateInactiveTab(10),
                                        WebStateList::INSERT_ACTIVATE,
                                        WebStateOpener());

  EXPECT_EQ(active_web_state_list->count(), 1);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  MoveTabsFromActiveToInactive(browser_active_.get(), browser_inactive_.get());

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 1);
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
  inactive_web_state_list->InsertWebState(0, CreateInactiveTab(10),
                                          WebStateList::INSERT_ACTIVATE,
                                          WebStateOpener());

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 1);

  MoveTabsFromInactiveToActive(browser_inactive_.get(), browser_active_.get());

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 1);
}

// Restore all inactive tab.
TEST_F(InactiveTabsUtilsTest, RestoreAllInactive) {
  WebStateList* active_web_state_list = browser_active_->GetWebStateList();
  WebStateList* inactive_web_state_list = browser_inactive_->GetWebStateList();

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 0);

  // Add a new inactive tab (10 days without activity) in the inactive browser.
  inactive_web_state_list->InsertWebState(0, CreateInactiveTab(10),
                                          WebStateList::INSERT_ACTIVATE,
                                          WebStateOpener());

  EXPECT_EQ(active_web_state_list->count(), 0);
  EXPECT_EQ(inactive_web_state_list->count(), 1);

  RestoreAllInactiveTabs(browser_inactive_.get(), browser_active_.get());

  EXPECT_EQ(active_web_state_list->count(), 1);
  EXPECT_EQ(inactive_web_state_list->count(), 0);
}
