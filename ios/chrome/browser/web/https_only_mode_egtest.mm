// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/security_interstitials/core/https_only_mode_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/metrics/metrics_app_interface.h"
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

namespace {

// net::EmbeddedTestServer handler that responds with the request's query as the
// title and body.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse);
  response->set_content_type("text/html");
  response->set_content("HTTP_RESPONSE");
  return std::move(response);
}

std::unique_ptr<net::test_server::HttpResponse> FakeHTTPSResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse);
  response->set_content_type("text/html");
  response->set_content("HTTPS_RESPONSE");
  return std::move(response);
}

}  // namespace

// Tests for HTTPS-Only Mode.
// The tests here use WKWebView and don't go through Chrome's net stack. Due to
// this, we can't serve valid HTTPS over the test server due to platform
// limitations on iOS. Instead, we use a faux-HTTPS server (goodHTTPSServer)
// which is just another HTTP_SERVER but runs on a different port and returns a
// different text than self.testServer. badHTTPSServer is a proper HTTPS_SERVER
// that just serves bad HTTPS responses.
@interface HttpsOnlyModeUpgradeTestCase : ChromeTestCase {
  std::unique_ptr<net::test_server::EmbeddedTestServer> _goodHTTPSServer;
  std::unique_ptr<net::test_server::EmbeddedTestServer> _badHTTPSServer;
}

// The EmbeddedTestServer instance that serves faux-good HTTPS requests for
// tests.
@property(nonatomic, readonly)
    net::test_server::EmbeddedTestServer* goodHTTPSServer;

// The EmbeddedTestServer instance that serves actual bad HTTPS requests for
// tests.
@property(nonatomic, readonly)
    net::test_server::EmbeddedTestServer* badHTTPSServer;
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
        base::BindRepeating(&StandardResponse));
  }
  return _badHTTPSServer.get();
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey clearBrowsingHistory];

  // Start the HTTP server.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(base::BindRepeating(&StandardResponse)));
  GREYAssertTrue(self.testServer->Start(), @"Test HTTP server failed to start");
  GREYAssertTrue(self.goodHTTPSServer->Start(),
                 @"Test good faux-HTTPS server failed to start.");
  GREYAssertTrue(self.badHTTPSServer->Start(),
                 @"Test bad HTTPS server failed to start.");

  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");

  [HttpsOnlyModeAppInterface
      setHTTPSPortForTesting:self.goodHTTPSServer->port()];
  [HttpsOnlyModeAppInterface setHTTPPortForTesting:self.testServer->port()];
  [HttpsOnlyModeAppInterface useFakeHTTPSForTesting:false];
}

- (void)tearDown {
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
}

#pragma mark - Tests

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

  // Reload. Since the URL is now allowlisted, this should immediately load
  // HTTP without trying to upgrade.
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:2
                        forHistogram:@(security_interstitials::https_only_mode::
                                           kEventHistogram)],
                @"Unexpected histogram event recorded.");
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

  // Tap "Go back" on the interstitial. This should go back to about:blank.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:"Revision"];

  // Go forward. Should hit the interstitial again.
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey
      waitForWebStateContainingText:"You are seeing this warning because this "
                                    "site does not support HTTPS"];
  [self assertFailedUpgrade:2];
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

@end
