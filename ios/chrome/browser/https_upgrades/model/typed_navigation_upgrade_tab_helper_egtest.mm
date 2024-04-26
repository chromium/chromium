// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/security_interstitials/core/https_only_mode_metrics.h"
#import "components/security_interstitials/core/omnibox_https_upgrade_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_app_interface.h"
#import "ios/chrome/browser/https_upgrades/model/https_upgrade_test_helper.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/components/security_interstitials/https_only_mode/feature.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util.h"

using security_interstitials::omnibox_https_upgrades::Event;
using security_interstitials::omnibox_https_upgrades::kEventHistogram;

namespace {

std::string GetURLWithoutScheme(const GURL& url) {
  return url.spec().substr(url.scheme().size() + strlen("://"));
}

}  // namespace

// Tests defaulting typed omnibox navigations to HTTPS.
@interface TypedNavigationUpgradeTestCase : HttpsUpgradeTestCaseBase {
}
@end

@implementation TypedNavigationUpgradeTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  config.features_enabled.push_back(omnibox::kDefaultTypedNavigationsToHttps);
  config.features_disabled.push_back(
      security_interstitials::features::kHttpsUpgrades);
  // TODO(crbug.com/335821156): Investigate why rich inline triggers with
  // test_HTTPWithSlowHTTPS_ShouldFallBack and remove this.
  config.features_disabled.push_back(omnibox::kRichAutocompletion);
  return config;
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey clearBrowsingHistory];

  // Disable HTTPS-Only Mode.
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kHttpsOnlyModeEnabled];
}

- (void)tearDown {
  [super tearDown];
}

// Asserts that the navigation wasn't upgraded.
- (void)assertNoUpgrade {
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:0
              forHistogram:@(security_interstitials::omnibox_https_upgrades::
                                 kEventHistogram)],
      @"Shouldn't record event histogram");
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"HTTPS Only Mode timer is unexpectedly running");
  GREYAssert(![HttpsUpgradeAppInterface isOmniboxUpgradeTimerRunning],
             @"Omnibox upgrade timer is unexpectedly running");

  // HTTPS-Only mode shouldn't handle this navigation.
  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:0
                        forHistogram:@(security_interstitials::https_only_mode::
                                           kEventHistogram)],
                @"HTTPS-Only mode unexpectedly recorded a histogram event");
}

// Asserts that the metrics are properly recorded for a successful upgrade.
- (void)assertSuccessfulUpgrade:(int)repeatCount {
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:2 * repeatCount
              forHistogram:@(security_interstitials::omnibox_https_upgrades::
                                 kEventHistogram)],
      @"Failed to record event histogram");

  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1 * repeatCount
             forBucket:static_cast<int>(
                           security_interstitials::omnibox_https_upgrades::
                               Event::kHttpsLoadStarted)
          forHistogram:@(security_interstitials::omnibox_https_upgrades::
                             kEventHistogram)],
      @"Failed to record upgrade attempt");
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1 * repeatCount
             forBucket:static_cast<int>(
                           security_interstitials::omnibox_https_upgrades::
                               Event::kHttpsLoadSucceeded)
          forHistogram:@(security_interstitials::omnibox_https_upgrades::
                             kEventHistogram)],
      @"Failed to record upgrade attempt");
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"HTTPS Only Mode timer is still running");
  GREYAssert(![HttpsUpgradeAppInterface isOmniboxUpgradeTimerRunning],
             @"Omnibox upgrade timer is still running");

  // HTTPS-Only mode shouldn't handle this navigation.
  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:0
                        forHistogram:@(security_interstitials::https_only_mode::
                                           kEventHistogram)],
                @"HTTPS-Only mode unexpectedly recorded a histogram event");
}

// Asserts that the metrics are properly recorded for a failed upgrade.
// repeatCount is the expected number of times the upgrade failed.
- (void)assertFailedUpgrade:(int)repeatCount {
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:(repeatCount * 2)
              forHistogram:@(security_interstitials::omnibox_https_upgrades::
                                 kEventHistogram)],
      @"Failed to record event histogram");

  GREYAssertNil(
      [MetricsAppInterface
           expectCount:repeatCount
             forBucket:static_cast<int>(
                           security_interstitials::omnibox_https_upgrades::
                               Event::kHttpsLoadStarted)
          forHistogram:@(security_interstitials::omnibox_https_upgrades::
                             kEventHistogram)],
      @"Failed to record upgrade attempt");
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:repeatCount
             forBucket:static_cast<int>(
                           security_interstitials::omnibox_https_upgrades::
                               Event::kHttpsLoadFailedWithCertError)
          forHistogram:@(security_interstitials::omnibox_https_upgrades::
                             kEventHistogram)],
      @"Failed to record fail event");
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"HTTPS Only Mode timer is still running");
  GREYAssert(![HttpsUpgradeAppInterface isOmniboxUpgradeTimerRunning],
             @"Omnibox upgrade timer is still running");

  // HTTPS-Only mode shouldn't handle this navigation.
  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:0
                        forHistogram:@(security_interstitials::https_only_mode::
                                           kEventHistogram)],
                @"HTTPS-Only mode unexpectedly recorded a histogram event");
}

// Asserts that the metrics are properly recorded for a timed-out upgrade.
// repeatCount is the expected number of times the upgrade failed.
- (void)assertTimedOutUpgrade:(int)repeatCount {
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:(repeatCount * 2)
              forHistogram:@(security_interstitials::omnibox_https_upgrades::
                                 kEventHistogram)],
      @"Incorrect numbber of records in event histogram");

  GREYAssertNil(
      [MetricsAppInterface
           expectCount:repeatCount
             forBucket:static_cast<int>(
                           security_interstitials::omnibox_https_upgrades::
                               Event::kHttpsLoadStarted)
          forHistogram:@(security_interstitials::omnibox_https_upgrades::
                             kEventHistogram)],
      @"Failed to record upgrade attempt");
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:repeatCount
             forBucket:static_cast<int>(
                           security_interstitials::omnibox_https_upgrades::
                               Event::kHttpsLoadTimedOut)
          forHistogram:@(security_interstitials::omnibox_https_upgrades::
                             kEventHistogram)],
      @"Failed to record fail event");
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"HTTPS Only Mode timer is still running");
  GREYAssert(![HttpsUpgradeAppInterface isOmniboxUpgradeTimerRunning],
             @"Omnibox upgrade timer is still running");

  // HTTPS-Only mode shouldn't handle this navigation.
  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:0
                        forHistogram:@(security_interstitials::https_only_mode::
                                           kEventHistogram)],
                @"HTTPS-Only mode unexpectedly recorded a histogram event");
}

#pragma mark - Tests

// Navigate to an HTTP URL. Since it's not typed in the omnibox, it shouldn't
// be upgraded to HTTPS.
- (void)test_NoTyping_ShouldNotUpgrade {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];

  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  [self assertNoUpgrade];
}

// Type an HTTP URL with scheme. It shouldn't be upgraded to HTTPS.
- (void)test_TypeFullHTTPURL_ShouldNotUpgrade {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];

  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:GURL("data:text/html,Blank Page")];
  [ChromeEarlGrey waitForWebStateContainingText:"Blank Page"];

  GURL testURL = self.goodHTTPSServer->GetURL("/");
  [ChromeEarlGreyUI typeTextInOmnibox:testURL.spec() andPressEnter:YES];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTPS_RESPONSE"];
  [self assertNoUpgrade];
}

// Type an HTTPS URL with scheme. It shouldn't be upgraded.
- (void)test_TypeFullHTTPSURL_ShouldNotUpgrade {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];

  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:GURL("data:text/html,Blank Page")];
  [ChromeEarlGrey waitForWebStateContainingText:"Blank Page"];

  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGreyUI typeTextInOmnibox:testURL.spec() andPressEnter:YES];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  [self assertNoUpgrade];
}

// Type an HTTP URL without scheme. The navigation should be upgraded to HTTPS
// which should load successfully.
- (void)test_TypeHTTPWithGoodHTTPS_ShouldUpgrade {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];

  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:GURL("data:text/html,Blank Page")];
  [ChromeEarlGrey waitForWebStateContainingText:"Blank Page"];

  GURL testURL = self.testServer->GetURL("/");
  std::string text = GetURLWithoutScheme(testURL);

  // Type the URL in the omnibox.
  [ChromeEarlGreyUI typeTextInOmnibox:text andPressEnter:YES];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTPS_RESPONSE"];
  [self assertSuccessfulUpgrade:1];

  // Load an interim data URL to clear the "HTTP_RESPONSE" text.
  [ChromeEarlGrey loadURL:GURL("data:text/html,Blank Page")];
  [ChromeEarlGrey waitForWebStateContainingText:"Blank Page"];

  // Type again. Normally, Omnibox should remember the successful HTTPS
  // navigation and not attempt to upgrade again. We are using a faux-HTTPS
  // server in tests which serves an http:// URL, so it will get upgraded again.
  [ChromeEarlGreyUI typeTextInOmnibox:text andPressEnter:YES];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTPS_RESPONSE"];
  [self assertSuccessfulUpgrade:2];
}

// Same as test_TypeHTTPWithGoodHTTPS_ShouldUpgrade but with HTTPS-Only Mode
// enabled.
- (void)test_TypeHTTPWithGoodHTTPS_HTTPSOnlyModeEnabled_ShouldUpgrade {
  // Enable HTTPS-Only Mode.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kHttpsOnlyModeEnabled];

  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];

  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:GURL("data:text/html,Blank Page")];
  [ChromeEarlGrey waitForWebStateContainingText:"Blank Page"];

  GURL testURL = self.testServer->GetURL("/");
  std::string text = GetURLWithoutScheme(testURL);

  // Type the URL in the omnibox.
  [ChromeEarlGreyUI typeTextInOmnibox:text andPressEnter:YES];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTPS_RESPONSE"];
  [self assertSuccessfulUpgrade:1];

  // Load an interim data URL to clear the "HTTP_RESPONSE" text.
  [ChromeEarlGrey loadURL:GURL("data:text/html,Blank Page")];
  [ChromeEarlGrey waitForWebStateContainingText:"Blank Page"];

  // Type again. Normally, Omnibox should remember the successful HTTPS
  // navigation and not attempt to upgrade again. We are using a faux-HTTPS
  // server in tests which serves an http:// URL, so it will get upgraded again.
  [ChromeEarlGreyUI typeTextInOmnibox:text andPressEnter:YES];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTPS_RESPONSE"];
  [self assertSuccessfulUpgrade:2];

  // HTTPS-Only mode shouldn't handle this navigation.
  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:0
                        forHistogram:@(security_interstitials::https_only_mode::
                                           kEventHistogram)],
                @"HTTPS-Only mode unexpectedly recorded a histogram event");
}

// Type an HTTP URL without scheme. The navigation should be upgraded to HTTPS,
// but the HTTPS URL serves bad response. The navigation should fall back to
// HTTP.
- (void)test_HTTPWithBadHTTPS_ShouldFallback {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.badHTTPSServer->port()
                                      useFakeHTTPS:false];
  [HttpsUpgradeAppInterface
      setFallbackHttpPortForTesting:self.testServer->port()];

  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:GURL("data:text/html,Blank Page")];
  [ChromeEarlGrey waitForWebStateContainingText:"Blank Page"];

  // Type the URL in the omnibox.
  GURL testURL = self.testServer->GetURL("/");
  std::string text = GetURLWithoutScheme(testURL);

  // Navigation should upgrade but eventually load the HTTP URL.
  [ChromeEarlGreyUI typeTextInOmnibox:text andPressEnter:YES];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  [self assertFailedUpgrade:1];

  // Load an interim data URL to clear the "HTTP_RESPONSE" text.
  [ChromeEarlGrey loadURL:GURL("data:text/html,Blank Page")];
  [ChromeEarlGrey waitForWebStateContainingText:"Blank Page"];

  // Try again. This time the omnibox will find a history match for the http
  // URL and navigate directly to it. Histograms shouldn't change.
  // TODO(crbug.com/40165447): We should try the https URL after a certain
  // time has passed.
  [ChromeEarlGreyUI typeTextInOmnibox:text andPressEnter:YES];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  [self assertFailedUpgrade:1];
}

// Type an HTTP URL without scheme. The navigation should be upgraded to HTTPS,
// but the HTTPS URL serves a slow loading response. The upgrade should timeout
// and the navigation should fall back to HTTP.
- (void)test_HTTPWithSlowHTTPS_ShouldFallBack {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.slowServer->port()
                                      useFakeHTTPS:true];
  [HttpsUpgradeAppInterface
      setFallbackHttpPortForTesting:self.testServer->port()];
  // Set the fallback delay to zero. This will immediately stop the HTTPS
  // upgrade attempt.
  [HttpsUpgradeAppInterface setFallbackDelayForTesting:0];

  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:GURL("data:text/html,Blank Page")];
  [ChromeEarlGrey waitForWebStateContainingText:"Blank Page"];

  // Type the URL in the omnibox with an \n at the end.
  GURL testURL = self.testServer->GetURL("/");
  std::string text = GetURLWithoutScheme(testURL);

  // Navigation should upgrade but eventually load the HTTP URL due to slow
  // HTTPS.
  [ChromeEarlGreyUI typeTextInOmnibox:text andPressEnter:YES];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  [self assertTimedOutUpgrade:1];

  // Load an interim data URL to clear the "HTTP_RESPONSE" text.
  [ChromeEarlGrey loadURL:GURL("data:text/html,Blank Page")];
  [ChromeEarlGrey waitForWebStateContainingText:"Blank Page"];

  // Try again. This time the omnibox will find a history match for the http
  // URL and navigate directly to it. Histograms shouldn't change.
  // TODO(crbug.com/40165447): We should try the https URL after a certain
  // time has passed.
  [ChromeEarlGreyUI typeTextInOmnibox:text andPressEnter:YES];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  [self assertTimedOutUpgrade:1];
}

// Regression test for crbug.com/1379605: This test checks that the fallback
// logic can handle redirects from HTTPS to HTTP. The test steps are:
// 1. Type a hostname. This loads an HTTPS URL that redirects to a slow loading
//    HTTP URL.
// 2. Navigation upgrade times out while on the HTTP URL. This initiates a
//    fallback navigation using the HTTP version of the URL in step 1.
// 3. The fallback HTTP URL immediately responds without redirecting, even
//    though it has a ?redirect parameter.
// Disabled due to crbug.com/1469971
- (void)DISABLED_test_HTTPSRedirectsToSlowHTTP_ShouldFallback {
  // Use the faux good HTTPS server and standard HTTP server for the test.
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];
  [HttpsUpgradeAppInterface
      setFallbackHttpPortForTesting:self.testServer->port()];

  // Set the fallback delay to zero. This will immediately stop the HTTPS
  // upgrade attempt.
  [HttpsUpgradeAppInterface setFallbackDelayForTesting:0];

  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:GURL("data:text/html,Blank Page")];
  [ChromeEarlGrey waitForWebStateContainingText:"Blank Page"];

  GURL slowHTTPURL = self.slowServer->GetURL("/");
  // Upgraded HTTPS URL is a redirect to the slow HTTP URL.
  GURL upgradedURL =
      self.goodHTTPSServer->GetURL("/?redirect=" + slowHTTPURL.spec());
  std::string text = GetURLWithoutScheme(upgradedURL);

  // Type the URL in the omnibox without a scheme and navigate.
  // Navigation will upgrade to HTTPS, then redirect to slow HTTP, then
  // timeout, then fallback to normal HTTP.
  // The fallback HTTP URL will immediately show a response.
  [ChromeEarlGreyUI typeTextInOmnibox:text andPressEnter:YES];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  [self assertTimedOutUpgrade:1];
}

@end
