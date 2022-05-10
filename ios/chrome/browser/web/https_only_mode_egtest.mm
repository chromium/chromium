// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/bind.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/metrics/metrics_app_interface.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/web/https_only_mode_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#include "ios/components/security_interstitials/https_only_mode/feature.h"
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

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

const long kVeryLongTimeout = 100 * 3600 * 1000;

// net::EmbeddedTestServer handler that responds with the request's query as the
// title and body.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    int* counter,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == "/") {
    std::unique_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    response->set_content_type("text/html");
    response->set_content("HTTP_RESPONSE");
    if (counter)
      (*counter)++;
    return std::move(response);
  }
  // Ignore everything else such as favicon URLs.
  return nullptr;
}

std::unique_ptr<net::test_server::HttpResponse> FakeHTTPSResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse);

  const GURL request_url = request.GetURL();
  const std::string destValue =
      base::UnescapeBinaryURLComponent(request_url.query_piece());
  // If the URL is in the form http://example.com/?redirect=url,
  // redirect the response to `url`.
  if (base::StartsWith(destValue, "redirect=")) {
    const std::string dest = destValue.substr(strlen("redirect="));
    response->set_code(net::HttpStatusCode::HTTP_MOVED_PERMANENTLY);
    response->AddCustomHeader("Location", dest);
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    response->set_content_type("text/html");
    response->set_content(base::StringPrintf(
        "<html><head></head><body>Redirecting to %s</body></html>",
        dest.c_str()));
    return response;
  }

  response->set_content_type("text/html");
  response->set_content("HTTPS_RESPONSE");
  return std::move(response);
}

std::unique_ptr<net::test_server::HttpResponse> FakeHungHTTPSResponse(
    const net::test_server::HttpRequest& request) {
  return std::make_unique<net::test_server::HungResponse>();
}

}  // namespace

// Tests for HTTPS-Only Mode.
// The tests here use WKWebView and don't go through Chrome's net stack. Due to
// this, we can't serve valid HTTPS over the test server due to platform
// limitations on iOS. Instead, we use a faux-HTTPS server (goodHTTPSServer)
// which is just another HTTP_SERVER but runs on a different port and returns a
// different text than self.testServer. badHTTPSServer is a proper HTTPS_SERVER
// that just serves bad HTTPS responses. slowHTTPSServer is a faux-HTTPS server
// that serves hung responses.
@interface HttpsOnlyModeUpgradeTestCase : ChromeTestCase {
  // Counts the number of HTTP responses returned by the test server. Doesn't
  // count the faux-HTTPS or bad-HTTPS responses. Used to check if prerender
  // navigations are successfully cancelled (the server shouldn't return a
  // response for them).
  int _HTTPResponseCounter;
  std::unique_ptr<net::test_server::EmbeddedTestServer> _goodHTTPSServer;
  std::unique_ptr<net::test_server::EmbeddedTestServer> _badHTTPSServer;
  std::unique_ptr<net::test_server::EmbeddedTestServer> _slowHTTPSServer;
}

// The EmbeddedTestServer instance that serves faux-good HTTPS responses for
// tests.
@property(nonatomic, readonly)
    net::test_server::EmbeddedTestServer* goodHTTPSServer;

// The EmbeddedTestServer instance that serves actual bad HTTPS responses for
// tests.
@property(nonatomic, readonly)
    net::test_server::EmbeddedTestServer* badHTTPSServer;

// The EmbeddedTestServer instance that serves a hung response for tests.
@property(nonatomic, readonly)
    net::test_server::EmbeddedTestServer* slowHTTPSServer;

// The value of the kHttpsOnlyModeEnabled pref, before this test
// started. Saved in order to restore it back to its original value after
// the test completes.
@property(nonatomic, assign) BOOL originalHttpsOnlyModeEnabled;

@end

@implementation HttpsOnlyModeUpgradeTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  config.features_enabled.push_back(
      security_interstitials::features::kHttpsOnlyMode);
  return config;
}

- (net::EmbeddedTestServer*)goodHTTPSServer {
  if (!_goodHTTPSServer) {
    _goodHTTPSServer = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTP);
    _goodHTTPSServer->RegisterRequestHandler(
        base::BindRepeating(&FakeHTTPSResponse));
  }
  return _goodHTTPSServer.get();
}

- (net::EmbeddedTestServer*)badHTTPSServer {
  if (!_badHTTPSServer) {
    _badHTTPSServer = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    _badHTTPSServer->RegisterRequestHandler(
        base::BindRepeating(&StandardResponse, nullptr));
  }
  return _badHTTPSServer.get();
}

- (net::EmbeddedTestServer*)slowHTTPSServer {
  if (!_slowHTTPSServer) {
    _slowHTTPSServer = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTP);
    _slowHTTPSServer->RegisterRequestHandler(
        base::BindRepeating(&FakeHungHTTPSResponse));
  }
  return _slowHTTPSServer.get();
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey clearBrowsingHistory];

  // Start the HTTP server.
  _HTTPResponseCounter = 0;
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      base::BindRepeating(&StandardResponse, &_HTTPResponseCounter)));

  GREYAssertTrue(self.testServer->Start(), @"Test HTTP server failed to start");
  GREYAssertTrue(self.goodHTTPSServer->Start(),
                 @"Test good faux-HTTPS server failed to start.");
  GREYAssertTrue(self.badHTTPSServer->Start(),
                 @"Test bad HTTPS server failed to start.");
  GREYAssertTrue(self.slowHTTPSServer->Start(),
                 @"Test slow faux-HTTPS server failed to start.");

  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");

  [HttpsOnlyModeAppInterface
      setHTTPSPortForTesting:self.goodHTTPSServer->port()];
  [HttpsOnlyModeAppInterface setHTTPPortForTesting:self.testServer->port()];
  [HttpsOnlyModeAppInterface useFakeHTTPSForTesting:false];
  [HttpsOnlyModeAppInterface setFallbackDelayForTesting:kVeryLongTimeout];

  self.originalHttpsOnlyModeEnabled =
      [ChromeEarlGrey userBooleanPref:prefs::kHttpsOnlyModeEnabled];
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kHttpsOnlyModeEnabled];
}

- (void)tearDown {
  [ChromeEarlGrey setBoolValue:self.originalHttpsOnlyModeEnabled
                   forUserPref:prefs::kHttpsOnlyModeEnabled];
  [HttpsOnlyModeAppInterface clearAllowlist];

  // Release the histogram tester.
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
  [super tearDown];
}

// Asserts that the metrics are properly recorded for a successful upgrade.
- (void)assertSuccessfulUpgrade {
  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:2
                        forHistogram:@(security_interstitials::https_only_mode::
                                           kEventHistogram)],
                @"Failed to record event histogram");

  GREYAssertNil([MetricsAppInterface
                     expectCount:1
                       forBucket:static_cast<int>(
                                     security_interstitials::https_only_mode::
                                         Event::kUpgradeAttempted)
                    forHistogram:@(security_interstitials::https_only_mode::
                                       kEventHistogram)],
                @"Failed to record upgrade attempt");
  GREYAssertNil([MetricsAppInterface
                     expectCount:1
                       forBucket:static_cast<int>(
                                     security_interstitials::https_only_mode::
                                         Event::kUpgradeSucceeded)
                    forHistogram:@(security_interstitials::https_only_mode::
                                       kEventHistogram)],
                @"Failed to record upgrade attempt");

  GREYAssert(![HttpsOnlyModeAppInterface isTimerRunning],
             @"Timer is still running");
}

// Asserts that the metrics are properly recorded for a failed upgrade.
// repeatCount is the expected number of times the upgrade failed.
- (void)assertFailedUpgrade:(int)repeatCount {
  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:(repeatCount * 2)
                        forHistogram:@(security_interstitials::https_only_mode::
                                           kEventHistogram)],
                @"Failed to record event histogram");

  GREYAssertNil([MetricsAppInterface
                     expectCount:repeatCount
                       forBucket:static_cast<int>(
                                     security_interstitials::https_only_mode::
                                         Event::kUpgradeAttempted)
                    forHistogram:@(security_interstitials::https_only_mode::
                                       kEventHistogram)],
                @"Failed to record upgrade attempt");
  GREYAssertNil([MetricsAppInterface
                     expectCount:repeatCount
                       forBucket:static_cast<int>(
                                     security_interstitials::https_only_mode::
                                         Event::kUpgradeFailed)
                    forHistogram:@(security_interstitials::https_only_mode::
                                       kEventHistogram)],
                @"Failed to record fail event");
  GREYAssert(![HttpsOnlyModeAppInterface isTimerRunning],
             @"Timer is still running");
}

// Asserts that the metrics are properly recorded for a timed-out upgrade.
// repeatCount is the expected number of times the upgrade failed.
- (void)assertTimedOutUpgrade:(int)repeatCount {
  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:(repeatCount * 2)
                        forHistogram:@(security_interstitials::https_only_mode::
                                           kEventHistogram)],
                @"Incorrect numbber of records in event histogram");

  GREYAssertNil([MetricsAppInterface
                     expectCount:repeatCount
                       forBucket:static_cast<int>(
                                     security_interstitials::https_only_mode::
                                         Event::kUpgradeAttempted)
                    forHistogram:@(security_interstitials::https_only_mode::
                                       kEventHistogram)],
                @"Failed to record upgrade attempt");
  GREYAssertNil([MetricsAppInterface
                     expectCount:repeatCount
                       forBucket:static_cast<int>(
                                     security_interstitials::https_only_mode::
                                         Event::kUpgradeTimedOut)
                    forHistogram:@(security_interstitials::https_only_mode::
                                       kEventHistogram)],
                @"Failed to record fail event");
  GREYAssert(![HttpsOnlyModeAppInterface isTimerRunning],
             @"Timer is still running");
}

#pragma mark - Tests

// Disable the feature and navigate to an HTTP URL directly. Since the feature
// is disabled, this should load the HTTP URL even though the upgraded HTTPS
// version serves good SSL.
- (void)testUpgrade_FeatureDisabled_NoUpgrade {
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kHttpsOnlyModeEnabled];

  [HttpsOnlyModeAppInterface setHTTPPortForTesting:self.testServer->port()];
  [HttpsOnlyModeAppInterface
      setHTTPSPortForTesting:self.goodHTTPSServer->port()];
  [HttpsOnlyModeAppInterface useFakeHTTPSForTesting:true];

  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:0
                        forHistogram:@(security_interstitials::https_only_mode::
                                           kEventHistogram)],
                @"Shouldn't record event histogram");
  GREYAssert(![HttpsOnlyModeAppInterface isTimerRunning],
             @"Timer is still running");
}

// Navigate to an HTTP URL directly. The upgraded HTTPS version serves good SSL.
// This should end up loading the HTTPS version of the URL.
- (void)testUpgrade_GoodHTTPS {
  [HttpsOnlyModeAppInterface setHTTPPortForTesting:self.testServer->port()];
  [HttpsOnlyModeAppInterface
      setHTTPSPortForTesting:self.goodHTTPSServer->port()];
  [HttpsOnlyModeAppInterface useFakeHTTPSForTesting:true];

  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTPS_RESPONSE"];
  [self assertSuccessfulUpgrade];
}

// Navigate to an HTTP URL by clicking a link. This should end up loading the
// HTTPS version of the URL.
- (void)testUpgrade_GoodHTTPS_LinkClick {
  [HttpsOnlyModeAppInterface setHTTPPortForTesting:self.testServer->port()];
  [HttpsOnlyModeAppInterface
      setHTTPSPortForTesting:self.goodHTTPSServer->port()];
  [HttpsOnlyModeAppInterface useFakeHTTPSForTesting:true];
  int HTTPPort = self.testServer->port();

  GURL testURL(base::StringPrintf(
      "data:text/html,"
      "<a href='http://127.0.0.1:%d/good-https' id='link'>Link</a><br>READY",
      HTTPPort));
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:"READY"];

  // Click on the http link. Should load the https URL.
  [ChromeEarlGrey tapWebStateElementWithID:@"link"];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTPS_RESPONSE"];
  [self assertSuccessfulUpgrade];
}

// Navigate to an HTTP URL directly. The upgraded HTTPS version serves good SSL
// which redirects to the original HTTP URL. This should show the interstitial.
- (void)testUpgrade_HTTPSRedirectsToHTTP {
  [HttpsOnlyModeAppInterface setHTTPPortForTesting:self.testServer->port()];
  [HttpsOnlyModeAppInterface
      setHTTPSPortForTesting:self.goodHTTPSServer->port()];
  [HttpsOnlyModeAppInterface useFakeHTTPSForTesting:true];

  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGrey waitForWebStateContainingText:"Revision"];

  GURL targetURL = self.testServer->GetURL("/");
  GURL upgradedURL =
      self.goodHTTPSServer->GetURL("/?redirect=" + targetURL.spec());
  const std::string port_str = base::NumberToString(self.testServer->port());
  GURL::Replacements replacements;
  replacements.SetPortStr(port_str);
  GURL testURL = upgradedURL.ReplaceComponents(replacements);

  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"You are seeing this warning because this "
                                    "site does not support HTTPS"];
  [self assertFailedUpgrade:1];

  // Click through the interstitial. This should load the HTTP page.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssert(![HttpsOnlyModeAppInterface isTimerRunning],
             @"Timer is still running");

  // Going back should go to chrome://version.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:"Revision"];
  [self assertFailedUpgrade:1];
}

// Tests that prerendered navigations that should be upgraded are cancelled.
// This test is adapted from testTapPrerenderSuggestions() in
// prerender_egtest.mm.
- (void)testUpgrade_BadHTTPS_PrerenderCanceled {
  // TODO(crbug.com/793306): Re-enable the test on iPad once the alternate
  // letters problem is fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(
        @"Disabled for iPad due to alternate letters educational screen.");
  }

  // TODO(crbug.com/1315304): Reenable.
  if ([ChromeEarlGrey isNewOmniboxPopupEnabled]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for new popup");
  }

  [HttpsOnlyModeAppInterface setHTTPPortForTesting:self.testServer->port()];
  [HttpsOnlyModeAppInterface
      setHTTPSPortForTesting:self.badHTTPSServer->port()];
  [HttpsOnlyModeAppInterface useFakeHTTPSForTesting:false];

  [ChromeEarlGrey clearBrowsingHistory];

  // Type the full URL. This will show an interstitial. This adds the URL to
  // history.
  GURL testURL = self.testServer->GetURL("/");
  NSString* pageString = base::SysUTF8ToNSString(testURL.GetContent());
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText([pageString stringByAppendingString:@"\n"])];
  [ChromeEarlGrey
      waitForWebStateContainingText:"You are seeing this warning because this "
                                    "site does not support HTTPS"];
  [self assertFailedUpgrade:1];
  GREYAssertEqual(1, _HTTPResponseCounter,
                  @"The page should have been loaded once");

  // Click through the interstitial.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssert(![HttpsOnlyModeAppInterface isTimerRunning],
             @"Timer is still running");
  GREYAssertEqual(2, _HTTPResponseCounter,
                  @"The page should have been loaded twice");

  // Close all tabs and reopen. This clears the allowlist because it's currently
  // per-tab.
  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];

  // Type the begining of the address to have the autocomplete suggestion.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  // Type a single character. This causes two prerender attempts.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText([pageString substringToIndex:1])];

  // Wait until prerender request reaches the server.
  bool prerendered = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return self->_HTTPResponseCounter > 2;
  });
  GREYAssertTrue(prerendered, @"Prerender did not happen");

  // Check the histograms. All prerender attempts must be cancelled. Relying on
  // the histogram here isn't great, but there doesn't seem to be a good
  // way of testing that prerenders have been cancelled.
  GREYAssertNil(
      [MetricsAppInterface expectCount:0
                             forBucket:/*PRERENDER_FINAL_STATUS_USED=*/0
                          forHistogram:@"Prerender.FinalStatus"],
      @"Prerender was used");
  // TODO(crbug.com/1302509): Check that the CANCEL bucket has non-zero
  // elements. Not currently supported by MetricsAppInterface.
}

// Navigate to an HTTP URL directly. The upgraded HTTPS version serves bad SSL.
// The upgrade will fail and the HTTPS-Only mode interstitial will be shown.
// Click through the interstitial, then reload the page. The HTTP page should
// be shown.
- (void)testUpgrade_BadHTTPS_ProceedInterstitial_Allowlisted {
  [HttpsOnlyModeAppInterface setHTTPPortForTesting:self.testServer->port()];
  [HttpsOnlyModeAppInterface
      setHTTPSPortForTesting:self.badHTTPSServer->port()];
  [HttpsOnlyModeAppInterface useFakeHTTPSForTesting:false];

  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"You are seeing this warning because this "
                                    "site does not support HTTPS"];
  [self assertFailedUpgrade:1];

  // Click through the interstitial. This should load the HTTP page.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssert(![HttpsOnlyModeAppInterface isTimerRunning],
             @"Timer is still running");

  // Reload. Since the URL is now allowlisted, this should immediately load
  // HTTP without trying to upgrade.
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:2
                        forHistogram:@(security_interstitials::https_only_mode::
                                           kEventHistogram)],
                @"Unexpected histogram event recorded.");
  GREYAssert(![HttpsOnlyModeAppInterface isTimerRunning],
             @"Timer is still running");

  // Allowlist decisions shouldn't carry over to incognito. Open an incognito
  // tab and try there.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"You are seeing this warning because this "
                                    "site does not support HTTPS"];
  // Click through the interstitial. This should load the HTTP page.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssert(![HttpsOnlyModeAppInterface isTimerRunning],
             @"Timer is still running");

  // Reload. Since the URL is now allowlisted, this should immediately load
  // HTTP without trying to upgrade.
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
}

// Same as testUpgrade_BadHTTPS_ProceedInterstitial_Allowlisted but uses
// a slow HTTPS response instead:
// Navigate to an HTTP URL directly. The upgraded HTTPS version serves a slow
// loading SSL page. The upgrade will be cancelled and the HTTPS-Only mode
// interstitial will be shown. Click through the interstitial, then reload the
// page. The HTTP page should be shown.
- (void)testUpgrade_SlowHTTPS_ProceedInterstitial_Allowlisted {
  [HttpsOnlyModeAppInterface setHTTPPortForTesting:self.testServer->port()];
  [HttpsOnlyModeAppInterface
      setHTTPSPortForTesting:self.slowHTTPSServer->port()];
  [HttpsOnlyModeAppInterface useFakeHTTPSForTesting:true];
  // Set the fallback delay to zero. This will immediately stop the HTTPS
  // upgrade attempt.
  [HttpsOnlyModeAppInterface setFallbackDelayForTesting:0];

  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"You are seeing this warning because this "
                                    "site does not support HTTPS"];
  [self assertTimedOutUpgrade:1];

  // Click through the interstitial. This should load the HTTP page.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssert(![HttpsOnlyModeAppInterface isTimerRunning],
             @"Timer is still running");

  // Reload. Since the URL is now allowlisted, this should immediately load
  // HTTP without trying to upgrade.
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:2
                        forHistogram:@(security_interstitials::https_only_mode::
                                           kEventHistogram)],
                @"Unexpected histogram event recorded.");
  GREYAssert(![HttpsOnlyModeAppInterface isTimerRunning],
             @"Timer is still running");
}

// Navigate to an HTTP URL directly. The upgraded HTTPS version serves bad SSL.
// The upgrade will fail and the HTTPS-Only mode interstitial will be shown.
// Tap Go back on the interstitial.
- (void)testUpgrade_BadHTTPS_GoBack {
  [HttpsOnlyModeAppInterface setHTTPPortForTesting:self.testServer->port()];
  [HttpsOnlyModeAppInterface
      setHTTPSPortForTesting:self.badHTTPSServer->port()];
  [HttpsOnlyModeAppInterface useFakeHTTPSForTesting:false];

  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGrey waitForWebStateContainingText:"Revision"];

  // Load a site with a bad HTTPS upgrade. This shows an interstitial.
  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"You are seeing this warning because this "
                                    "site does not support HTTPS"];
  [self assertFailedUpgrade:1];

  // Tap "Go back" on the interstitial. This should go back to chrome://version.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:"Revision"];

  // Go forward. Should hit the interstitial again.
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey
      waitForWebStateContainingText:"You are seeing this warning because this "
                                    "site does not support HTTPS"];
  [self assertFailedUpgrade:2];
}

// Same as testUpgrade_BadHTTPS_GoBack but uses a slow HTTPS response instead:
// Navigate to an HTTP URL directly. The upgraded HTTPS version serves a slow
// loading HTTPS page. The upgrade will be cancelled and the HTTPS-Only mode
// interstitial will be shown. Tap Go back on the interstitial.
- (void)testUpgrade_SlowHTTPS_GoBack {
  [HttpsOnlyModeAppInterface setHTTPPortForTesting:self.testServer->port()];
  [HttpsOnlyModeAppInterface
      setHTTPSPortForTesting:self.slowHTTPSServer->port()];
  [HttpsOnlyModeAppInterface useFakeHTTPSForTesting:true];
  // Set the fallback delay to zero. This will immediately stop the HTTPS
  // upgrade attempt.
  [HttpsOnlyModeAppInterface setFallbackDelayForTesting:0];

  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGrey waitForWebStateContainingText:"Revision"];

  // Load a site with a slow HTTPS upgrade. This shows an interstitial.
  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"You are seeing this warning because this "
                                    "site does not support HTTPS"];
  [self assertTimedOutUpgrade:1];

  // Tap "Go back" on the interstitial. This should go back to
  // chrome://version.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:"Revision"];

  // Go forward. Should hit the interstitial again.
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey
      waitForWebStateContainingText:"You are seeing this warning because this "
                                    "site does not support HTTPS"];
  [self assertTimedOutUpgrade:2];
}

// Navigate to an HTTP URL and click through the interstitial. Then,
// navigate to a new page and go back. This should load the HTTP URL
// without showing the interstitial again.
- (void)testUpgrade_BadHTTPS_GoBackToAllowlistedSite {
  [HttpsOnlyModeAppInterface setHTTPPortForTesting:self.testServer->port()];
  [HttpsOnlyModeAppInterface
      setHTTPSPortForTesting:self.badHTTPSServer->port()];
  [HttpsOnlyModeAppInterface useFakeHTTPSForTesting:false];

  [ChromeEarlGrey loadURL:GURL("about:blank")];

  // Load a site with a bad HTTPS upgrade. This shows an interstitial.
  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"You are seeing this warning because this "
                                    "site does not support HTTPS"];
  [self assertFailedUpgrade:1];

  // Click through the interstitial. This should load the HTTP page.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];

  // Go to a new page.
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGrey waitForWebStateContainingText:"Revision"];

  // Then go back to the HTTP URL. Since we previously clicked through its
  // interstitial, this should immediately load the HTTP response.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  // Histogram numbers shouldn't change.
  [self assertFailedUpgrade:1];
}

// Same as testUpgrade_BadHTTPS_GoBackToAllowlistedSite but uses a slow
// HTTPS response instead:
// Navigate to an HTTP URL with a slow HTTPS upgrade, click through the
// interstitial. Then, navigate to a new page and go back. This should load the
// HTTP URL without showing the interstitial again.
- (void)testUpgrade_SlowHTTPS_GoBackToAllowlistedSite {
  [HttpsOnlyModeAppInterface setHTTPPortForTesting:self.testServer->port()];
  [HttpsOnlyModeAppInterface
      setHTTPSPortForTesting:self.slowHTTPSServer->port()];
  [HttpsOnlyModeAppInterface useFakeHTTPSForTesting:true];
  // Set the fallback delay to zero. This will immediately stop the HTTPS
  // upgrade attempt.
  [HttpsOnlyModeAppInterface setFallbackDelayForTesting:0];

  [ChromeEarlGrey loadURL:GURL("about:blank")];

  // Load a site with a bad HTTPS upgrade. This shows an interstitial.
  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"You are seeing this warning because this "
                                    "site does not support HTTPS"];
  [self assertTimedOutUpgrade:1];

  // Click through the interstitial. This should load the HTTP page.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssert(![HttpsOnlyModeAppInterface isTimerRunning],
             @"Timer is still running");

  // Go to a new page.
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGrey waitForWebStateContainingText:"Revision"];

  // Then go back to the HTTP URL. Since we previously clicked through its
  // interstitial, this should immediately load the HTTP response.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  // Histogram numbers shouldn't change.
  [self assertTimedOutUpgrade:1];
}

@end
