// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#import "components/page_info/core/page_info_action.h"
#import "components/strings/grit/components_branded_strings.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/ui/page_info/features.h"
#import "ios/chrome/browser/ui/page_info/page_info_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

@interface PageInfoSecurityTestCase : ChromeTestCase
@end

@implementation PageInfoSecurityTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  config.features_enabled.push_back(kRevampPageInfoIos);
  return config;
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey clearBrowsingHistory];
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to set up histogram tester.");
}

- (void)tearDown {
  [super tearDown];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
}

// Navigates to Page Info's Security Subpage.
- (void)openSecuritySubpage {
  [ChromeEarlGreyUI openPageInfo];

  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_PAGE_INFO_SECURITY_STATUS_NOT_SECURE))]
      performAction:grey_tap()];
}

// Returns a matcher for Page Info's Security Subpage back button.
- (id<GREYMatcher>)securityBackButton {
  return grey_allOf(
      testing::NavigationBarBackButton(),
      grey_ancestor(grey_accessibilityID(
          kPageInfoSecurityViewNavigationBarAccessibilityIdentifier)),
      nil);
}

// Tests that the correct connection label is displayed and that the learn more
// button opens a help center article.
- (void)testSecurity {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];

  [self openSecuritySubpage];

  // Check that "Connection is Not Secure” is displayed.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_PAGE_INFO_SECURITY_CONNECTION_STATUS_NOT_SECURE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_PAGE_INFO_NOT_SECURE_DETAILS))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the Learn more row.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_LEARN_MORE))]
      performAction:grey_tap()];

  // Check that the help center article was opened.
  GREYAssertEqual(std::string("support.google.com"),
                  [ChromeEarlGrey webStateVisibleURL].host(),
                  @"Did not navigate to the help center article.");

  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:page_info::PAGE_INFO_CONNECTION_HELP_OPENED
          forHistogram:base::SysUTF8ToNSString(
                           page_info::kWebsiteSettingsActionHistogram)],
      @"WebsiteSettings.Action histogram not logged.");
}

// Tests that rotating the device will don't dismiss the security view and that
// the navigation bar is still visible.
- (void)testShowSecurityRotation {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];

  [self openSecuritySubpage];

  // Check that the page info view has appeared.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kPageInfoSecurityViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kPageInfoSecurityViewNavigationBarAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the navigation bar has both the security's page title and the
  // page URL.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_PAGE_INFO_SECURITY))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text([NSString
                     stringWithCString:[ChromeEarlGrey webStateVisibleURL]
                                           .host()
                                           .c_str()
                              encoding:[NSString defaultCStringEncoding]])]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Rotate the device and check that the page info view is still presented
  // along with the navigation bar.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                error:nil];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kPageInfoSecurityViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kPageInfoSecurityViewNavigationBarAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that closing the security view, by tapping on the done button,
// dismisses both the security view and the page info view.
- (void)testCloseSecurity {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];

  [self openSecuritySubpage];

  // Tap on the navigation done button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Check that neither the security view nor the page info view are visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kPageInfoSecurityViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPageInfoViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that going back on the security view, by tapping on back button,
// dismisses the security view and shows page info view.
- (void)testBackToPageInfo {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/")];

  [self openSecuritySubpage];

  // Tap on the navigation back button.
  [[EarlGrey selectElementWithMatcher:[self securityBackButton]]
      performAction:grey_tap()];

  // Check that security view is no longer visible but that the page info view
  // is visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kPageInfoSecurityViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPageInfoViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
