// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/app_launcher/app_launcher_coordinator.h"

#import <UIKit/UIKit.h>

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for AppLauncherCoordinator class.
class AppLauncherCoordinatorTest : public PlatformTest {
 protected:
  AppLauncherCoordinatorTest() {
    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];
    coordinator_ = [[AppLauncherCoordinator alloc]
        initWithBaseViewController:base_view_controller_];
    application_ = OCMClassMock([UIApplication class]);
    OCMStub([application_ sharedApplication]).andReturn(application_);
    AppLauncherTabHelper::CreateForWebState(&web_state_, nil, nil);
  }
  ~AppLauncherCoordinatorTest() override { [application_ stopMocking]; }

  AppLauncherTabHelper* tab_helper() {
    return AppLauncherTabHelper::FromWebState(&web_state_);
  }

  web::TestWebState web_state_;
  UIViewController* base_view_controller_ = nil;
  ScopedKeyWindow scoped_key_window_;
  AppLauncherCoordinator* coordinator_ = nil;
  id application_ = nil;
};


// Tests that an itunes URL shows an alert.
TEST_F(AppLauncherCoordinatorTest, ItmsUrlShowsAlert) {
  BOOL app_exists = [coordinator_ appLauncherTabHelper:tab_helper()
                                      launchAppWithURL:GURL("itms://1234")
                                        linkTransition:NO];
  EXPECT_TRUE(app_exists);
  ASSERT_TRUE([base_view_controller_.presentedViewController
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alert_controller =
      base::mac::ObjCCastStrict<UIAlertController>(
          base_view_controller_.presentedViewController);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_IN_ANOTHER_APP),
              alert_controller.message);
}

// Tests that in the new AppLauncher, an app URL attempts to launch the
// application.
TEST_F(AppLauncherCoordinatorTest, AppUrlLaunchesApp) {
  OCMExpect([application_ openURL:[NSURL URLWithString:@"some-app://1234"]
                          options:@{}
                completionHandler:nil]);
  [coordinator_ appLauncherTabHelper:tab_helper()
                    launchAppWithURL:GURL("some-app://1234")
                      linkTransition:YES];
  [application_ verify];
}

// Tests that in the new AppLauncher, an app URL shows a prompt if there was no
// link transition.
TEST_F(AppLauncherCoordinatorTest, AppUrlShowsPrompt) {
  [coordinator_ appLauncherTabHelper:tab_helper()
                    launchAppWithURL:GURL("some-app://1234")
                      linkTransition:NO];
  ASSERT_TRUE([base_view_controller_.presentedViewController
      isKindOfClass:[UIAlertController class]]);
  UIAlertController* alert_controller =
      base::mac::ObjCCastStrict<UIAlertController>(
          base_view_controller_.presentedViewController);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPEN_IN_ANOTHER_APP),
              alert_controller.message);
}
