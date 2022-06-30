// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/security_interstitials/core/omnibox_https_upgrade_metrics.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/https_upgrades/https_upgrade_app_interface.h"
#import "ios/chrome/browser/https_upgrades/https_upgrade_test_helper.h"
#include "ios/chrome/browser/metrics/metrics_app_interface.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/testing/embedded_test_server_handlers.h"
#include "ios/web/common/features.h"
#include "ios/web/public/test/element_selector.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using security_interstitials::omnibox_https_upgrades::Event;
using security_interstitials::omnibox_https_upgrades::kEventHistogram;

namespace {

std::string GetURLWithoutScheme(const GURL& url) {
  return url.spec().substr(url.scheme().size() + strlen("://"));
}

}  // namespace

// Tests defaulting typed omnibox navigations to HTTPS.
@interface TypedNavigationUpgradeTestCase : HttpsUpgradeTestCase {
}
@end

@implementation TypedNavigationUpgradeTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  config.features_enabled.push_back(omnibox::kDefaultTypedNavigationsToHttps);
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
}

// Focuses on the omnibox and types the given text.
- (void)typeTextAndPressEnter:(const std::string&)text {
  [ChromeEarlGreyUI focusOmnibox];
  // Type the text.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(base::SysUTF8ToNSString(text))];

  // Press enter to navigate.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"\n")];
}

// Navigate to an HTTP URL. Since it's not typed in the omnibox, it shouldn't
// be upgraded to HTTPS.
- (void)testUpgrade_NoTyping_NoUpgrade {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];

  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  [self assertNoUpgrade];
}

// Type an HTTP URL with scheme. It shouldn't be upgraded to HTTPS.
- (void)testUpgrade_TypeFullHTTPURL_NoUpgrade {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];

  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:GURL("data:text/html,Blank Page")];
  [ChromeEarlGrey waitForWebStateContainingText:"Blank Page"];

  GURL testURL = self.goodHTTPSServer->GetURL("/");
  [self typeTextAndPressEnter:testURL.spec()];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTPS_RESPONSE"];
  [self assertNoUpgrade];
}

// Type an HTTPS URL with scheme. It shouldn't be upgraded.
- (void)testUpgrade_TypeFullHTTPSURL_NoUpgrade {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];

  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:GURL("data:text/html,Blank Page")];
  [ChromeEarlGrey waitForWebStateContainingText:"Blank Page"];

  GURL testURL = self.testServer->GetURL("/");
  [self typeTextAndPressEnter:testURL.spec()];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  [self assertNoUpgrade];
}

// Type an HTTP URL without scheme. The navigation should be upgraded to HTTPS
// which should load successfully.
- (void)testUpgrade_GoodHTTPS {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];

  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:GURL("data:text/html,Blank Page")];
  [ChromeEarlGrey waitForWebStateContainingText:"Blank Page"];

  GURL testURL = self.testServer->GetURL("/");
  std::string text = GetURLWithoutScheme(testURL);

  // Type the URL in the omnibox.
  [self typeTextAndPressEnter:text];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTPS_RESPONSE"];
  [self assertSuccessfulUpgrade:1];

  // Load an interim data URL to clear the "HTTP_RESPONSE" text.
  [ChromeEarlGrey loadURL:GURL("data:text/html,Blank Page")];
  [ChromeEarlGrey waitForWebStateContainingText:"Blank Page"];

  // Type again. Normally, Omnibox should remember the successful HTTPS
  // navigation and not attempt to upgrade again. We are using a faux-HTTPS
  // server in tests which serves an http:// URL, so it will get upgraded again.
  [self typeTextAndPressEnter:text];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTPS_RESPONSE"];
  [self assertSuccessfulUpgrade:2];
}

// Type an HTTP URL without scheme. The navigation should be upgraded to HTTPS,
// but the HTTPS URL serves bad response. The navigation should fall back to
// HTTP.
- (void)testUpgrade_BadHTTPS {
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

  [self typeTextAndPressEnter:text];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  [self assertFailedUpgrade:1];

  // Load an interim data URL to clear the "HTTP_RESPONSE" text.
  [ChromeEarlGrey loadURL:GURL("data:text/html,Blank Page")];
  [ChromeEarlGrey waitForWebStateContainingText:"Blank Page"];

  // Try again. This time the omnibox will find a history match for the http
  // URL and navigate directly to it. Histograms shouldn't change.
  // TODO(crbug.com/1169564): We should try the https URL after a certain
  // time has passed.
  [self typeTextAndPressEnter:text];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  [self assertFailedUpgrade:1];
}

@end
