// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activities/request_desktop_or_mobile_site_activity.h"

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/lens/lens_browser_agent.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for covering the RequestDesktopOrMobileSiteActivity class.
class RequestDesktopOrMobileSiteActivityTest : public PlatformTest {
 protected:
  RequestDesktopOrMobileSiteActivityTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    LensBrowserAgent::CreateForBrowser(browser_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = WebNavigationBrowserAgent::FromBrowser(browser_.get());
    WebStateOpener opener;
    auto web_state = std::make_unique<web::FakeWebState>();
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state->SetNavigationManager(std::move(navigation_manager));
    browser_->GetWebStateList()->InsertWebState(
        0, std::move(web_state), WebStateList::InsertionFlags::INSERT_ACTIVATE,
        opener);
  }

  void SetUp() override {
    PlatformTest::SetUp();

    mocked_handler_ =
        OCMStrictProtocolMock(@protocol(BrowserCoordinatorCommands));
  }

  // Creates a RequestDesktopOrMobileSiteActivity instance.
  RequestDesktopOrMobileSiteActivity* CreateActivity(
      web::UserAgentType user_agent) {
    return [[RequestDesktopOrMobileSiteActivity alloc]
        initWithUserAgent:user_agent
                  handler:mocked_handler_
          navigationAgent:agent_];
  }

  id mocked_handler_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  WebNavigationBrowserAgent* agent_;
  // Navigation manager for the web state at index 0 in `browser_`'s web state
  // list.
  web::FakeNavigationManager* navigation_manager_;
};

// Tests that the activity cannot be performed when the user agent is NONE.
TEST_F(RequestDesktopOrMobileSiteActivityTest, UserAgentNone_ActivityDisabled) {
  RequestDesktopOrMobileSiteActivity* activity =
      CreateActivity(web::UserAgentType::NONE);
  EXPECT_FALSE([activity canPerformWithActivityItems:@[]]);
}

// Tests that the activity is enabled, has the right title and triggers the
// right action when the user agent is Desktop.
TEST_F(RequestDesktopOrMobileSiteActivityTest, UserAgentDesktop) {
  RequestDesktopOrMobileSiteActivity* activity =
      CreateActivity(web::UserAgentType::DESKTOP);

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);

  NSString* requestMobileString =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_REQUEST_MOBILE_SITE);
  EXPECT_TRUE([requestMobileString isEqualToString:activity.activityTitle]);

  [activity performActivity];

  [mocked_handler_ verify];
  [activity_partial_mock verify];
  EXPECT_TRUE(navigation_manager_->RequestMobileSiteWasCalled());
}

// Tests that the activity is enabled, has the right title and triggers the
// right action when the user agent is Mobile.
TEST_F(RequestDesktopOrMobileSiteActivityTest, UserAgentMobile) {
  [[mocked_handler_ expect] showDefaultSiteViewIPH];

  RequestDesktopOrMobileSiteActivity* activity =
      CreateActivity(web::UserAgentType::MOBILE);

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);

  NSString* requestDesktopString =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_REQUEST_DESKTOP_SITE);
  EXPECT_TRUE([requestDesktopString isEqualToString:activity.activityTitle]);

  [activity performActivity];

  [mocked_handler_ verify];
  [activity_partial_mock verify];
  EXPECT_TRUE(navigation_manager_->RequestDesktopSiteWasCalled());
}
