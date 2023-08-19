// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/supervised_user/core/browser/supervised_user_url_filter.h"
#import "components/supervised_user/core/common/features.h"
#import "ios/chrome/browser/signin/capabilities_types.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/settings_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

// Tests the core user journeys of a supervised user with FamilyLink parental
// control restrictions enabled.
@interface SupervisedUserWithParentalControlsTestCase : ChromeTestCase
@end

@implementation SupervisedUserWithParentalControlsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      supervised_user::kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
  return config;
}

- (void)setUp {
  [super setUp];
  bool started = self.testServer->Start();
  GREYAssertTrue(started, @"Test server failed to start.");
}

- (void)tearDown {
  [SettingsAppInterface resetSupervisedUserURLFilterBehavior];

  [super tearDown];
}

- (void)testSupervisedUserSignin {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGrey setIsSubjectToParentalControls:YES forIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

- (void)testSupervisedUserWithAllSitesRestricted {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGrey setIsSubjectToParentalControls:YES forIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  GURL safeURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey loadURL:safeURL];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];

  // Select site filter option "only allow approved sites".
  [SettingsAppInterface setSupervisedUserURLFilterBehavior:
                            supervised_user::SupervisedUserURLFilter::BLOCK];
  // Ensure that the supervised user block page interstitial is displayed.
  [ChromeEarlGrey waitForWebStateContainingText:"Ask your parent"];

  // Reloading the page should not affect the interstitial.
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:"Ask your parent"];
}

@end
