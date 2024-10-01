// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activities/request_desktop_or_mobile_site_activity.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/lens/model/lens_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

// Test fixture for covering the RequestDesktopOrMobileSiteActivity class.
class RequestDesktopOrMobileSiteActivityTest : public PlatformTest {
 protected:
  RequestDesktopOrMobileSiteActivityTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    LensBrowserAgent::CreateForBrowser(browser_.get());
    WebNavigationBrowserAgent::CreateForBrowser(browser_.get());
    agent_ = WebNavigationBrowserAgent::FromBrowser(browser_.get());
    auto web_state = std::make_unique<web::FakeWebState>();
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state->SetNavigationManager(std::move(navigation_manager));
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  void SetUp() override {
    PlatformTest::SetUp();

    mocked_handler_ = OCMStrictProtocolMock(@protocol(HelpCommands));
  }

  // Creates a RequestDesktopOrMobileSiteActivity instance.
  RequestDesktopOrMobileSiteActivity* CreateActivity(
      web::UserAgentType user_agent) {
    return [[RequestDesktopOrMobileSiteActivity alloc]
        initWithUserAgent:user_agent
              helpHandler:mocked_handler_
          navigationAgent:agent_];
  }

  id mocked_handler_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebNavigationBrowserAgent> agent_;
  // Navigation manager for the web state at index 0 in `browser_`'s web state
  // list.
  raw_ptr<web::FakeNavigationManager> navigation_manager_;
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
  EXPECT_NSEQ(requestMobileString, activity.activityTitle);

  [activity performActivity];

  [mocked_handler_ verify];
  [activity_partial_mock verify];
  EXPECT_TRUE(navigation_manager_->RequestMobileSiteWasCalled());
}

// Tests that the activity is enabled, has the right title and triggers the
// right action when the user agent is Mobile.
TEST_F(RequestDesktopOrMobileSiteActivityTest, UserAgentMobile) {
  [[mocked_handler_ expect]
      presentInProductHelpWithType:InProductHelpType::kDefaultSiteView];

  RequestDesktopOrMobileSiteActivity* activity =
      CreateActivity(web::UserAgentType::MOBILE);

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);

  NSString* requestDesktopString =
      l10n_util::GetNSString(IDS_IOS_SHARE_MENU_REQUEST_DESKTOP_SITE);
  EXPECT_NSEQ(requestDesktopString, activity.activityTitle);

  [activity performActivity];

  [mocked_handler_ verify];
  [activity_partial_mock verify];
  EXPECT_TRUE(navigation_manager_->RequestDesktopSiteWasCalled());
}
