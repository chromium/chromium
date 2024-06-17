// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/account_switching/account_switching_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

// Integration tests using the Account Switching menu.
@interface AccountSwitchingTestCase : WebHttpServerChromeTestCase
@end

@implementation AccountSwitchingTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  config.features_enabled.push_back(kIdentityDiscAccountSwitch);

  return config;
}

- (void)testViewAccountSwitchingMenu {
  // Sign in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Select the identity disc particle.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kNTPFeedHeaderIdentityDisc)]
      performAction:grey_tap()];

  // Ensure the Account Switching menu is displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kAccountSwitchingTableViewId),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testCloseButtonAccountSwitchingMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"The close button exists only on iPhones.");
  }

  // Sign in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Select the identity disc particle.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kNTPFeedHeaderIdentityDisc)]
      performAction:grey_tap()];

  // Ensure the Account Switching menu is displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kAccountSwitchingTableViewId),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the Close button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountSwitchingCloseButtonId)]
      performAction:grey_tap()];

  // Verify the Account Switching menu is dismissed.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kAccountSwitchingTableViewId),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notVisible()];
}

@end
