// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/app_launcher/app_launcher_coordinator.h"

#import <UIKit/UIKit.h>

#include "base/mac/foundation_util.h"
#include "base/test/scoped_feature_list.h"
#include "ios/chrome/browser/app_launcher/app_launcher_flags.h"
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
  }

  ~AppLauncherCoordinatorTest() override { [application_ stopMocking]; }

  UIViewController* base_view_controller_ = nil;
  ScopedKeyWindow scoped_key_window_;
  AppLauncherCoordinator* coordinator_ = nil;
  id application_ = nil;
};


// Tests that an itunes URL shows an alert.
TEST_F(AppLauncherCoordinatorTest, ItmsUrlShowsAlert) {
  BOOL app_exists = [coordinator_ appLauncherTabHelper:nullptr
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

// Tests that an app URL attempts to launch the application.
TEST_F(AppLauncherCoordinatorTest, AppUrlLaunchesApp) {
  // Make sure that the new AppLauncherRefresh logic is disabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kAppLauncherRefresh);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  OCMExpect([application_ openURL:[NSURL URLWithString:@"some-app://1234"]]);
#pragma clang diagnostic pop
  [coordinator_ appLauncherTabHelper:nullptr
                    launchAppWithURL:GURL("some-app://1234")
                      linkTransition:NO];
  [application_ verify];
}

// Tests that in the new AppLauncher, an app URL attempts to launch the
// application.
TEST_F(AppLauncherCoordinatorTest, AppLauncherRefreshAppUrlLaunchesApp) {
  // Make sure that the new AppLauncherRefresh logic is enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kAppLauncherRefresh);
  OCMExpect([application_ openURL:[NSURL URLWithString:@"some-app://1234"]
                          options:@{}
                completionHandler:nil]);
  [coordinator_ appLauncherTabHelper:nullptr
                    launchAppWithURL:GURL("some-app://1234")
                      linkTransition:YES];
  [application_ verify];
}

// Tests that in the new AppLauncher, an app URL shows a prompt if there was no
// link transition.
TEST_F(AppLauncherCoordinatorTest, AppLauncherRefreshAppUrlShowsPrompt) {
  // Make sure that the new AppLauncherRefresh logic is enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kAppLauncherRefresh);
  [coordinator_ appLauncherTabHelper:nullptr
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

// Tests that |-appLauncherTabHelper:launchAppWithURL:linkTransition:| returns
// NO if there is no application that corresponds to a given URL.
TEST_F(AppLauncherCoordinatorTest, NoApplicationForUrl) {
  // Make sure that the new AppLauncherRefresh logic is disabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kAppLauncherRefresh);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  OCMStub(
      [application_ openURL:[NSURL URLWithString:@"no-app-installed://1234"]])
      .andReturn(NO);
#pragma clang diagnostic pop
  BOOL app_exists =
      [coordinator_ appLauncherTabHelper:nullptr
                        launchAppWithURL:GURL("no-app-installed://1234")
                          linkTransition:NO];
  EXPECT_FALSE(app_exists);
}
