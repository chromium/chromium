// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/policy/policy_constants.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/policy/model/scoped_policy_list.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

using chrome_test_util::IncognitoInterstitialCancelButton;
using chrome_test_util::IncognitoInterstitialLabelForURL;
using chrome_test_util::IncognitoInterstitialMatcher;
using chrome_test_util::IncognitoInterstitialOpenInChromeButton;
using chrome_test_util::IncognitoInterstitialOpenInChromeIncognitoButton;
using chrome_test_util::NTPIncognitoView;

@interface IncognitoInterstitialTestCase : ChromeTestCase
@end

@implementation IncognitoInterstitialTestCase {
  ScopedPolicyList scopedPolicies;
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey setBoolValue:YES
             forLocalStatePref:prefs::kIncognitoInterstitialEnabled];

  // Set Incognito Mode to "available",
  // as this is an assumption for most of these tests.
  scopedPolicies.SetPolicy(static_cast<int>(IncognitoModePrefs::kEnabled),
                           policy::key::kIncognitoModeAvailability);

  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

- (void)tearDown {
  scopedPolicies.Reset();
  [ChromeEarlGrey setBoolValue:NO
             forLocalStatePref:prefs::kIncognitoInterstitialEnabled];
  [super tearDown];
}

// Test the "Open in Chrome Incognito" journey through the Incognito
// interstitial.
- (void)testOpenInIncognitoFromNTP {
  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey openNewIncognitoTab];

  // Starting from Incognito NTP, loading a new URL.
  GURL destinationURL = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey sceneOpenURL:destinationURL];
  // Wait for the interstitial to appear.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:IncognitoInterstitialMatcher()];
  // Check the appropriate subtitle is sufficiently visible within the
  // Interstitial.
  [[EarlGrey selectElementWithMatcher:IncognitoInterstitialLabelForURL(
                                          destinationURL.spec())]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Tap the "Open in Chrome Incognito" button.
  [[EarlGrey selectElementWithMatcher:
                 IncognitoInterstitialOpenInChromeIncognitoButton()]
      performAction:grey_tap()];
  // Wait for the interstitial to disappear.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:IncognitoInterstitialMatcher()];
  // Wait for the expected page content to be displayed.
  [ChromeEarlGrey waitForWebStateContainingText:"You've arrived"];
  // Wait for the Incognito tab count to be one, as expected.
  [ChromeEarlGrey waitForIncognitoTabCount:1];
}

// Test the "Open in Chrome" journey through the Incognito interstitial.
- (void)testOpenInChromeFromNTP {
  // Starting from NTP, loading a new URL.
  GURL destinationURL = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey sceneOpenURL:destinationURL];
  // Wait for the interstitial to appear.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:IncognitoInterstitialMatcher()];
  // Check the appropriate subtitle is sufficiently visible within the
  // Interstitial.
  [[EarlGrey selectElementWithMatcher:IncognitoInterstitialLabelForURL(
                                          destinationURL.spec())]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Tap the "Open in Chrome" button.
  [[EarlGrey selectElementWithMatcher:IncognitoInterstitialOpenInChromeButton()]
      performAction:grey_tap()];
  // Wait for the interstitial to disappear.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:IncognitoInterstitialMatcher()];
  // Wait for the expected page content to be displayed.
  [ChromeEarlGrey waitForWebStateContainingText:"You've arrived"];
  // Wait for the main tab count to be one, as expected.
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Test the "Open in Chrome" journey starting from an already opened tab.
- (void)testOpenInChromeFromTab {
  // Go from NTP to some other web page.
  [ChromeEarlGrey loadURL:GURL("https://invalid")];

  // Starting from this regular tab, loading a new URL.
  GURL destinationURL = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey sceneOpenURL:destinationURL];
  // Wait for the interstitial to appear.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:IncognitoInterstitialMatcher()];
  // Check the appropriate subtitle is sufficiently visible within the
  // Interstitial.
  [[EarlGrey selectElementWithMatcher:IncognitoInterstitialLabelForURL(
                                          destinationURL.spec())]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Tap the "Open in Chrome" button.
  [[EarlGrey selectElementWithMatcher:IncognitoInterstitialOpenInChromeButton()]
      performAction:grey_tap()];
  // Wait for the interstitial to disappear.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:IncognitoInterstitialMatcher()];
  // Wait for the expected page content to be displayed.
  [ChromeEarlGrey waitForWebStateContainingText:"You've arrived"];
  // Wait for the main tab count to be two, as expected.
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Test the "Open in Chrome Incognito" journey starting from the tab switcher.
- (void)testOpenInChromeIncognitoFromTabSwitcher {
  // Close the NTP to go to the tab switcher.
  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey waitForMainTabCount:0];

  // Starting from the tab switcher, loading a new URL.
  GURL destinationURL = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey sceneOpenURL:destinationURL];
  // Wait for the interstitial to appear.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:IncognitoInterstitialMatcher()];
  // Check the appropriate subtitle is sufficiently visible within the
  // Interstitial.
  [[EarlGrey selectElementWithMatcher:IncognitoInterstitialLabelForURL(
                                          destinationURL.spec())]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Tap the "Open in Chrome Incognito" button.
  [[EarlGrey selectElementWithMatcher:
                 IncognitoInterstitialOpenInChromeIncognitoButton()]
      performAction:grey_tap()];
  // Wait for the interstitial to disappear.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:IncognitoInterstitialMatcher()];
  // Wait for the expected page content to be displayed.
  [ChromeEarlGrey waitForWebStateContainingText:"You've arrived"];
  // Wait for the main tab count to be two, as expected.
  [ChromeEarlGrey waitForIncognitoTabCount:1];
}

// Test the "Cancel" button of the Incognito Interstitial.
- (void)testCancelButton {
  [ChromeEarlGrey openNewIncognitoTab];

  // Starting from this regular tab, loading a new URL.
  GURL destinationURL = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey sceneOpenURL:destinationURL];
  // Wait for the interstitial to appear.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:IncognitoInterstitialMatcher()];
  // Check the appropriate subtitle is sufficiently visible within the
  // Interstitial.
  [[EarlGrey selectElementWithMatcher:IncognitoInterstitialLabelForURL(
                                          destinationURL.spec())]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Tap the Cancel button.
  [[EarlGrey selectElementWithMatcher:IncognitoInterstitialCancelButton()]
      performAction:grey_tap()];
  // Wait for the interstitial to disappear.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:IncognitoInterstitialMatcher()];
  // Wait for the Incognito tab count to be one, as expected.
  [ChromeEarlGrey waitForIncognitoTabCount:1];
  // Check the Incognito NTP is back.
  [[EarlGrey selectElementWithMatcher:NTPIncognitoView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Test that a new intent triggers the dismissal of a former instance of the
// Interstitial, then displays an Interstitial with the new URL.
- (void)testNewInterstitialReplacesFormerInterstitial {
  // Starting from NTP, loading a new URL.
  GURL destinationURL = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey sceneOpenURL:destinationURL];
  // Wait for the interstitial to appear.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:IncognitoInterstitialMatcher()];
  // Check the appropriate subtitle is sufficiently visible within the
  // Interstitial.
  [[EarlGrey selectElementWithMatcher:IncognitoInterstitialLabelForURL(
                                          destinationURL.spec())]
      assertWithMatcher:grey_sufficientlyVisible()];
  // While the Interstitial is shown, loading an alternative URL.
  GURL alternativeURL = self.testServer->GetURL("/chromium_logo_page.html");
  [ChromeEarlGrey sceneOpenURL:alternativeURL];
  // Wait for the interstitial to appear.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:IncognitoInterstitialMatcher()];
  // Check the appropriate subtitle is sufficiently visible within the
  // Interstitial.
  [[EarlGrey selectElementWithMatcher:IncognitoInterstitialLabelForURL(
                                          alternativeURL.spec())]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Tap the "Open in Chrome Incognito" button.
  [[EarlGrey selectElementWithMatcher:
                 IncognitoInterstitialOpenInChromeIncognitoButton()]
      performAction:grey_tap()];
  // Wait for the interstitial to disappear.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:IncognitoInterstitialMatcher()];
  // Wait for the expected page content to be displayed.
  [ChromeEarlGrey waitForWebStateContainingText:
                      "Page with some text and the chromium logo image."];
  // Wait for the Incognito tab count to be one, as expected.
  [ChromeEarlGrey waitForIncognitoTabCount:1];
}

// Test the interstitial is not presented when Incognito Mode is disabled
// through Enterprise policy.
- (void)testInterstitialIsNotPresentedWhenIncognitoModeIsDisabled {
  // Disabling Incognito mode.
  ScopedPolicyList scopedIncognitoModeDisabled;
  scopedIncognitoModeDisabled.SetPolicy(
      static_cast<int>(IncognitoModePrefs::kDisabled),
      policy::key::kIncognitoModeAvailability);

  // Close the NTP to go to the tab switcher.
  [ChromeEarlGrey closeCurrentTab];

  // Starting from NTP, loading a new URL.
  GURL destinationURL = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey sceneOpenURL:destinationURL];
  // Wait for the expected page content to be displayed.
  base::TimeDelta timeout = base::Seconds(20);
  [ChromeEarlGrey waitForWebStateContainingText:"You've arrived"
                                        timeout:timeout];
  // Wait for the Incognito tab count to be one, as expected.
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Test the interstitial is not presented when Incognito Mode is forced
// through Enterprise policy.
- (void)testInterstitialIsNotPresentedWhenIncognitoModeIsForced {
  // Forcing Incognito mode.
  ScopedPolicyList scopedIncognitoModeForced;
  scopedIncognitoModeForced.SetPolicy(
      static_cast<int>(IncognitoModePrefs::kForced),
      policy::key::kIncognitoModeAvailability);

  // Close the NTP to go to the tab switcher.
  [ChromeEarlGrey closeCurrentTab];

  // Starting from NTP, loading a new URL.
  GURL destinationURL = self.testServer->GetURL("/destination.html");
  [ChromeEarlGrey sceneOpenURL:destinationURL];
  // Wait for the expected page content to be displayed.
  base::TimeDelta timeout = base::Seconds(20);
  [ChromeEarlGrey waitForWebStateContainingText:"You've arrived"
                                        timeout:timeout];
  // Wait for the Incognito tab count to be one, as expected.
  [ChromeEarlGrey waitForIncognitoTabCount:1];
}

@end
