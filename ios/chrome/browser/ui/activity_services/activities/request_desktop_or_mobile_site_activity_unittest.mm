// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/request_desktop_or_mobile_site_activity.h"

#include "ios/chrome/browser/ui/commands/browser_commands.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for covering the RequestDesktopOrMobileSiteActivity class.
class RequestDesktopOrMobileSiteActivityTest : public PlatformTest {
 protected:
  RequestDesktopOrMobileSiteActivityTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    mocked_handler_ = OCMStrictProtocolMock(@protocol(BrowserCommands));
  }

  // Creates a RequestDesktopOrMobileSiteActivity instance.
  RequestDesktopOrMobileSiteActivity* CreateActivity(
      web::UserAgentType user_agent) {
    return [[RequestDesktopOrMobileSiteActivity alloc]
        initWithUserAgent:user_agent
                  handler:mocked_handler_];
  }

  id mocked_handler_;
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
  [[mocked_handler_ expect] requestMobileSite];

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
}

// Tests that the activity is enabled, has the right title and triggers the
// right action when the user agent is Mobile.
TEST_F(RequestDesktopOrMobileSiteActivityTest, UserAgentMobile) {
  [[mocked_handler_ expect] requestDesktopSite];

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
}
