// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>
#include <map>
#include <memory>

#include "base/bind.h"
#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "build/branding_buildflags.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/app/main_controller.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ClearBrowsingDataButton;
using chrome_test_util::ClearBrowsingDataView;
using chrome_test_util::ClearBrowsingHistoryButton;
using chrome_test_util::ClearCacheButton;
using chrome_test_util::ClearCookiesButton;
using chrome_test_util::ClearSavedPasswordsButton;
using chrome_test_util::ContentSettingsButton;
using chrome_test_util::SettingsCollectionView;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SettingsMenuPrivacyButton;
using chrome_test_util::VoiceSearchButton;

namespace {

const char kUrl[] = "http://foo/browsing";
const char kUrlWithSetCookie[] = "http://foo/set_cookie";
const char kResponse[] = "bar";
const char kResponseWithSetCookie[] = "bar with set cookie";
NSString* const kCookieName = @"name";
NSString* const kCookieValue = @"value";

enum MetricsServiceType {
  kMetrics,
  kBreakpad,
  kBreakpadFirstLaunch,
};

// Matcher for the Clear Browsing Data cell on the Privacy screen.
id<GREYMatcher> ClearBrowsingDataCell() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
}
// Matcher for the Search Engine cell on the main Settings screen.
id<GREYMatcher> SearchEngineButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_SEARCH_ENGINE_SETTING_TITLE);
}
// Matcher for the payment methods cell on the main Settings screen.
id<GREYMatcher> PaymentMethodsButton() {
  return ButtonWithAccessibilityLabelId(IDS_AUTOFILL_PAYMENT_METHODS);
}
// Matcher for the addresses cell on the main Settings screen.
id<GREYMatcher> AddressesButton() {
  return ButtonWithAccessibilityLabelId(IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE);
}
// Matcher for the Google Chrome cell on the main Settings screen.
id<GREYMatcher> GoogleChromeButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_PRODUCT_NAME);
}

// Matcher for the Preload Webpages button on the bandwidth UI.
id<GREYMatcher> BandwidthPreloadWebpagesButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_OPTIONS_PRELOAD_WEBPAGES);
}
// Matcher for the Privacy Handoff button on the privacy UI.
id<GREYMatcher> PrivacyHandoffButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES);
}
// Matcher for the Privacy Block Popups button on the privacy UI.
id<GREYMatcher> BlockPopupsButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_BLOCK_POPUPS);
}
// Matcher for the Bandwidth Settings button on the main Settings screen.
id<GREYMatcher> BandwidthSettingsButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_BANDWIDTH_MANAGEMENT_SETTINGS);
}

}  // namespace

// Settings tests for Chrome.
@interface SettingsTestCase : ChromeTestCase
@end

@implementation SettingsTestCase

- (void)tearDown {
  // It is possible for a test to fail with a menu visible, which can cause
  // future tests to fail.

  // Check if a sub-menu is still displayed. If so, close it.
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      assertWithMatcher:grey_notNil()
                  error:&error];
  if (!error) {
    [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
        performAction:grey_tap()];
  }

  // Check if the Settings menu is displayed. If so, close it.
  error = nil;
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      assertWithMatcher:grey_notNil()
                  error:&error];
  if (!error) {
    [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
        performAction:grey_tap()];
  }

  [super tearDown];
}

// Closes a sub-settings menu, and then the general Settings menu.
- (void)closeSubSettingsMenu {
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Performs the steps to clear browsing data. Must be called on the
// Clear Browsing Data settings screen, after having selected the data types
// scheduled for removal.
- (void)clearBrowsingData {
  [ChromeEarlGreyUI tapClearBrowsingDataMenuButton:ClearBrowsingDataButton()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          ConfirmClearBrowsingDataButton()]
      performAction:grey_tap()];

  // Before returning, make sure that the top of the Clear Browsing Data
  // settings screen is visible to match the state at the start of the method.
  // TODO(crbug.com/973708): On iOS 13 the settings menu appears as a card that
  // can be dismissed with a downward swipe.  This make it difficult to use a
  // gesture to return to the top of the Clear Browsing Data screen, so scroll
  // programatically instead. Remove this custom action if we switch back to a
  // fullscreen presentation.
  GREYPerformBlock scrollToTopBlock =
      ^BOOL(id element, __strong NSError** error) {
        UIScrollView* view = base::mac::ObjCCastStrict<UIScrollView>(element);
        view.contentOffset = CGPointZero;
        return YES;
      };
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      performAction:[GREYActionBlock actionWithName:@"Scroll to top"
                                       performBlock:scrollToTopBlock]];
}

// From the NTP, clears the cookies and site data via the UI.
- (void)clearCookiesAndSiteData {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI tapPrivacyMenuButton:ClearBrowsingDataCell()];

  // "Browsing history", "Cookies, Site Data" and "Cached Images and Files"
  // are the default checked options when the prefs are registered. Uncheck
  // "Browsing history" and "Cached Images and Files".
  // Prefs are reset to default at the end of each test.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      performAction:grey_tap()];

  [self clearBrowsingData];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// From the NTP, clears the saved passwords via the UI.
- (void)clearPasswords {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI tapPrivacyMenuButton:ClearBrowsingDataCell()];

  // "Browsing history", "Cookies, Site Data" and "Cached Images and Files"
  // are the default checked options when the prefs are registered. Unckeck all
  // of them and check "Passwords".
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      performAction:grey_tap()];

  [self clearBrowsingData];

  // Re-tap all the previously tapped cells, so that the default state of the
  // checkmarks is preserved.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Restore the Clear Browsing Data checkmarks prefs to their default state.
- (void)restoreClearBrowsingDataCheckmarksToDefault {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  PrefService* preferences = browserState->GetPrefs();
  preferences->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistory, true);
  preferences->SetBoolean(browsing_data::prefs::kDeleteCache, true);
  preferences->SetBoolean(browsing_data::prefs::kDeleteCookies, true);
  preferences->SetBoolean(browsing_data::prefs::kDeletePasswords, false);
  preferences->SetBoolean(browsing_data::prefs::kDeleteFormData, false);
}

- (void)setMetricsReportingEnabled:(BOOL)reportingEnabled {
  chrome_test_util::SetBooleanLocalStatePref(
      metrics::prefs::kMetricsReportingEnabled, reportingEnabled);
  // Breakpad uses dispatch_async to update its state. Wait to get to a
  // consistent state.
  chrome_test_util::WaitForBreakpadQueue();
}

// TODO(crbug.com/953862): Remove as part of feature flag cleanup.
- (void)setMetricsReportingEnabled:(BOOL)reportingEnabled
                          wifiOnly:(BOOL)wifiOnly {
  chrome_test_util::SetBooleanLocalStatePref(
      metrics::prefs::kMetricsReportingEnabled, reportingEnabled);
  chrome_test_util::SetBooleanLocalStatePref(prefs::kMetricsReportingWifiOnly,
                                             wifiOnly);
  // Breakpad uses dispatch_async to update its state. Wait to get to a
  // consistent state.
  chrome_test_util::WaitForBreakpadQueue();
}

// Checks for a given service that it is both recording and uploading, where
// appropriate.
- (void)assertMetricsServiceEnabled:(MetricsServiceType)serviceType {
  switch (serviceType) {
    case kMetrics:
      GREYAssertTrue(chrome_test_util::IsMetricsRecordingEnabled(),
                     @"Metrics recording should be enabled.");
      GREYAssertTrue(chrome_test_util::IsMetricsReportingEnabled(),
                     @"Metrics reporting should be enabled.");
      break;
    case kBreakpad:
      GREYAssertTrue(chrome_test_util::IsBreakpadEnabled(),
                     @"Breakpad should be enabled.");
      GREYAssertTrue(chrome_test_util::IsBreakpadReportingEnabled(),
                     @"Breakpad reporting should be enabled.");
      break;
    case kBreakpadFirstLaunch:
      // For first launch after upgrade, or after install, uploading of crash
      // reports is always disabled.  Check that the first launch flag is being
      // honored.
      GREYAssertTrue(chrome_test_util::IsBreakpadEnabled(),
                     @"Breakpad should be enabled.");
      GREYAssertFalse(chrome_test_util::IsBreakpadReportingEnabled(),
                      @"Breakpad reporting should be disabled.");
      break;
  }
}

// Checks for a given service that it is completely disabled.
- (void)assertMetricsServiceDisabled:(MetricsServiceType)serviceType {
  switch (serviceType) {
    case kMetrics: {
      GREYAssertFalse(chrome_test_util::IsMetricsRecordingEnabled(),
                      @"Metrics recording should be disabled.");
      GREYAssertFalse(chrome_test_util::IsMetricsReportingEnabled(),
                      @"Metrics reporting should be disabled.");
      break;
    }
    case kBreakpad:
    case kBreakpadFirstLaunch: {
      // Check only whether or not breakpad is enabled.  Disabling
      // breakpad does stop uploading, and does not change the flag
      // used to check whether or not it's uploading.
      GREYAssertFalse(chrome_test_util::IsBreakpadEnabled(),
                      @"Breakpad should be disabled.");
      break;
    }
  }
}

// Checks for a given service that it is recording, but not uploading anything.
// Used to test that the wifi-only preference is honored when the device is
// using a cellular network.
- (void)assertMetricsServiceEnabledButNotUploading:
    (MetricsServiceType)serviceType {
  switch (serviceType) {
    case kMetrics: {
      GREYAssertTrue(chrome_test_util::IsMetricsRecordingEnabled(),
                     @"Metrics recording should be enabled.");
      GREYAssertFalse(chrome_test_util::IsMetricsReportingEnabled(),
                      @"Metrics reporting should be disabled.");
      break;
    }
    case kBreakpad:
    case kBreakpadFirstLaunch: {
      GREYAssertTrue(chrome_test_util::IsBreakpadEnabled(),
                     @"Breakpad should be enabled.");
      GREYAssertFalse(chrome_test_util::IsBreakpadReportingEnabled(),
                      @"Breakpad reporting should be disabled.");
      break;
    }
  }
}

- (void)assertsMetricsPrefsForService:(MetricsServiceType)serviceType {
  // Two preferences, each with two values - on or off.  Check all four
  // combinations:
  // kMetricsReportingEnabled OFF and kMetricsReportingWifiOnly OFF
  //  - Services do not record data and do not upload data.

  // kMetricsReportingEnabled OFF and kMetricsReportingWifiOnly ON
  //  - Services do not record data and do not upload data.
  //    Note that if kMetricsReportingEnabled is OFF, the state of
  //    kMetricsReportingWifiOnly does not matter.

  // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly ON
  //  - Services record data and upload data only when the device is using
  //    a wifi connection.  Note:  rather than checking for wifi, the code
  //    checks for a cellular network (wwan).  wwan != wifi.  So if wwan is
  //    true, services do not upload any data.

  // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly OFF
  //  - Services record data and upload data.

  if ([ChromeEarlGrey isUMACellularEnabled]) {
    // kMetricsReportingEnabled OFF
    [self setMetricsReportingEnabled:NO];
  } else {
    // kMetricsReportingEnabled OFF and kMetricsReportingWifiOnly OFF
    [self setMetricsReportingEnabled:NO wifiOnly:NO];
  }
  // Service should be completely disabled.
  // I.e. no recording of data, and no uploading of what's been recorded.
  [self assertMetricsServiceDisabled:serviceType];

  if ([ChromeEarlGrey isUMACellularEnabled]) {
    // kMetricsReportingEnabled OFF
    [self setMetricsReportingEnabled:NO];
  } else {
    // kMetricsReportingEnabled OFF and kMetricsReportingWifiOnly ON
    [self setMetricsReportingEnabled:NO wifiOnly:YES];
  }
  // If kMetricsReportingEnabled is OFF, any service should remain completely
  // disabled, i.e. no uploading even if kMetricsReportingWifiOnly is ON.
  [self assertMetricsServiceDisabled:serviceType];

// Split here:  Official build vs. Development build.
// Official builds allow recording and uploading of data, honoring the
// metrics prefs.  Development builds should never record or upload data.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Official build.
  // The values of the prefs and the wwan vs wifi state should be honored by
  // the services, turning on and off according to the rules laid out above.

  if (![ChromeEarlGrey isUMACellularEnabled]) {
    // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly ON.
    [self setMetricsReportingEnabled:YES wifiOnly:YES];
    // Service should be enabled.
    [self assertMetricsServiceEnabled:serviceType];

    // Set the network to use a cellular network, which should disable uploading
    // when the wifi-only flag is set.
    chrome_test_util::SetWWANStateTo(YES);
    chrome_test_util::WaitForBreakpadQueue();
    [self assertMetricsServiceEnabledButNotUploading:serviceType];

    // Turn off cellular network usage, which should enable uploading.
    chrome_test_util::SetWWANStateTo(NO);
    chrome_test_util::WaitForBreakpadQueue();
    [self assertMetricsServiceEnabled:serviceType];

    // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly OFF
    [self setMetricsReportingEnabled:YES wifiOnly:NO];
    [self assertMetricsServiceEnabled:serviceType];
  }

#else
  // Development build.  Do not allow any recording or uploading of data.
  // Specifically, the kMetricsReportingEnabled preference is completely
  // disregarded for non-official builds, and checking its value always returns
  // false (NO).
  // This tests that no matter the state change, pref or network connection,
  // services remain disabled.

  if (![ChromeEarlGrey isUMACellularEnabled]) {
    // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly ON
    [self setMetricsReportingEnabled:YES wifiOnly:YES];
    // Service should remain disabled.
    [self assertMetricsServiceDisabled:serviceType];

    // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly OFF
    [self setMetricsReportingEnabled:YES wifiOnly:NO];
    // Service should remain disabled.
    [self assertMetricsServiceDisabled:serviceType];
  }
#endif
}

#pragma mark Tests

// Tests that clearing the cookies through the UI does clear all of them. Use a
// local server to navigate to a page that sets then tests a cookie, and then
// clears the cookie and tests it is not set.
- (void)testClearCookies {
  // Creates a map of canned responses and set up the test HTML server.
  std::map<GURL, std::pair<std::string, std::string>> response;

  NSString* cookieForURL =
      [NSString stringWithFormat:@"%@=%@", kCookieName, kCookieValue];

  response[web::test::HttpServer::MakeUrl(kUrlWithSetCookie)] =
      std::pair<std::string, std::string>(base::SysNSStringToUTF8(cookieForURL),
                                          kResponseWithSetCookie);
  response[web::test::HttpServer::MakeUrl(kUrl)] =
      std::pair<std::string, std::string>("", kResponse);

  web::test::SetUpSimpleHttpServerWithSetCookies(response);

  // Load |kUrl| and check that cookie is not set.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUrl)];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse];

  NSDictionary* cookies = [ChromeEarlGrey cookies];
  GREYAssertEqual(0U, cookies.count, @"No cookie should be found.");

  // Visit |kUrlWithSetCookie| to set a cookie and then load |kUrl| to check it
  // is still set.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUrlWithSetCookie)];
  [ChromeEarlGrey waitForWebStateContainingText:kResponseWithSetCookie];
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUrl)];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse];

  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kCookieValue, cookies[kCookieName],
                         @"Failed to set cookie.");
  GREYAssertEqual(1U, cookies.count, @"Only one cookie should be found.");

  // Restore the Clear Browsing Data checkmarks prefs to their default state
  // in Teardown.
  __weak SettingsTestCase* weakSelf = self;
  [self setTearDownHandler:^{
    [weakSelf restoreClearBrowsingDataCheckmarksToDefault];
  }];

  // Clear all cookies.
  [self clearCookiesAndSiteData];

  // Reload and test that there are no cookies left.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUrl)];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse];

  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqual(0U, cookies.count, @"No cookie should be found.");

  [ChromeEarlGrey closeAllTabs];
}

// Verifies that metrics reporting works properly under possible settings of the
// preferences kMetricsReportingEnabled and kMetricsReportingWifiOnly.
- (void)testMetricsReporting {
  [self assertsMetricsPrefsForService:kMetrics];
}

// Verifies that breakpad reporting works properly under possible settings of
// the preferences |kMetricsReportingEnabled| and |kMetricsReportingWifiOnly|
// for non-first-launch runs.
// NOTE: breakpad only allows uploading for non-first-launch runs.
- (void)testBreakpadReporting {
  [self setTearDownHandler:^{
    // Restore the first launch state to previous state.
    chrome_test_util::SetFirstLaunchStateTo(
        chrome_test_util::IsFirstLaunchAfterUpgrade());
  }];

  chrome_test_util::SetFirstLaunchStateTo(NO);
  [self assertsMetricsPrefsForService:kBreakpad];
}

// Verifies that breakpad reporting works properly under possible settings of
// the preferences |kMetricsReportingEnabled| and |kMetricsReportingWifiOnly|
// for first-launch runs.
// NOTE: breakpad only allows uploading for non-first-launch runs.
- (void)testBreakpadReportingFirstLaunch {
  [self setTearDownHandler:^{
    // Restore the first launch state to previous state.
    chrome_test_util::SetFirstLaunchStateTo(
        chrome_test_util::IsFirstLaunchAfterUpgrade());
  }];

  chrome_test_util::SetFirstLaunchStateTo(YES);
  [self assertsMetricsPrefsForService:kBreakpadFirstLaunch];
}

// Verifies that Settings opens when signed-out and in Incognito mode.
// This tests that crbug.com/607335 has not regressed.
- (void)testSettingsSignedOutIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Verifies that the Settings screen can be swiped down to dismiss, and clean up
// is performed allowing a new presentation.
- (void)testSettingsSwipeDownDismiss {
  if (!base::ios::IsRunningOnOrLater(13, 0, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 12 and lower.");
  }

  [ChromeEarlGreyUI openSettingsMenu];

  // Check that Settings is presented.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that Settings has been dismissed.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_nil()];

  // Re-Open Settings to confirm SwipeDown cleaned up properly and Settings can
  // be shown again.
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_notNil()];
}

// Verifies the UI elements are accessible on the Settings page.
- (void)testAccessibilityOnSettingsPage {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Verifies the UI elements are accessible on the Content Settings page.
- (void)testAccessibilityOnContentSettingsPage {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:ContentSettingsButton()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Content Settings
// Block Popups page.
- (void)testAccessibilityOnContentSettingsBlockPopupsPage {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:ContentSettingsButton()];
  [[EarlGrey selectElementWithMatcher:BlockPopupsButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Privacy Settings page.
- (void)testAccessibilityOnPrivacySettingsPage {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Privacy Handoff Settings
// page.
- (void)testAccessibilityOnPrivacyHandoffSettingsPage {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [[EarlGrey selectElementWithMatcher:PrivacyHandoffButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Privacy Clear Browsing Data
// Settings page.
- (void)testAccessibilityOnPrivacyClearBrowsingHistoryPage {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI tapPrivacyMenuButton:ClearBrowsingDataCell()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Bandwidth Management Settings
// page.
- (void)testAccessibilityOnBandwidthManagementSettingsPage {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:BandwidthSettingsButton()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Bandwidth Preload Webpages
// Settings page.
- (void)testAccessibilityOnBandwidthPreloadWebpagesSettingsPage {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:BandwidthSettingsButton()];
  [[EarlGrey selectElementWithMatcher:BandwidthPreloadWebpagesButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Search engine page.
- (void)testAccessibilityOnSearchEngine {
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:SearchEngineButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the payment methods page.
- (void)testAccessibilityOnPaymentMethods {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PaymentMethodsButton()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the addresses page.
- (void)testAccessibilityOnAddresses {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:AddressesButton()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the About Chrome page.
- (void)testAccessibilityOnGoogleChrome {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleChromeButton()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies the UI elements are accessible on the Voice Search page.
- (void)testAccessibilityOnVoiceSearch {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:VoiceSearchButton()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [self closeSubSettingsMenu];
}

// Verifies that the Settings UI registers keyboard commands when presented, but
// not when it itslef presents something.
- (void)testSettingsKeyboardCommands {
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_notNil()];

  // Verify that the Settings register keyboard commands.
  UIViewController* viewController =
      chrome_test_util::GetMainController()
          .interfaceProvider.mainInterface.viewController;
  UIViewController* settings = viewController.presentedViewController;
  GREYAssertNotNil(settings.keyCommands,
                   @"Settings should register key commands when presented.");

  // Present the Sign-in UI.
  id<GREYMatcher> matcher = grey_allOf(chrome_test_util::PrimarySignInButton(),
                                       grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  // Wait for UI to finish loading the Sign-in screen.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Verify that the Settings register keyboard commands.
  GREYAssertNil(settings.keyCommands,
                @"Settings should not register key commands when presented.");

  // Cancel the sign-in operation.
  [[EarlGrey selectElementWithMatcher:
                 grey_buttonTitle([l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)
                     uppercaseString])] performAction:grey_tap()];

  // Wait for UI to finish closing the Sign-in screen.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Verify that the Settings register keyboard commands.
  GREYAssertNotNil(settings.keyCommands,
                   @"Settings should register key commands when presented.");
}

@end
