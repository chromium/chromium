// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/functional/bind.h"
#import "base/strings/escape.h"
#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/security_interstitials/core/https_only_mode_metrics.h"
#import "components/security_interstitials/core/omnibox_https_upgrade_metrics.h"
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

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

const char kInterstitialText[] =
    "You are seeing this warning because this site does not support HTTPS";

enum class TestType {
  kHttpsOnlyMode,
  kHttpsUpgrades,
};

}  // namespace

// Tests for HTTPS-Only Mode.
// TODO(crbug.com/40849153): Remove the "ZZZ" when the bug is fixed.
@interface ZZZ_HttpsOnlyModeTestCase : HttpsUpgradeTestCaseBase {
}
@end

@implementation ZZZ_HttpsOnlyModeTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  config.features_enabled.push_back(
      security_interstitials::features::kHttpsOnlyMode);

  config.features_disabled.push_back(
      security_interstitials::features::kHttpsUpgrades);
  // Disable omnibox navigation upgrades.
  // typed_navigation_upgrade_tab_helper_egtest.mm already has a
  // test case with both features enabled.
  // (test_TypeHTTPWithGoodHTTPS_HTTPSOnlyModeEnabled_ShouldUpgrade)
  config.features_disabled.push_back(omnibox::kDefaultTypedNavigationsToHttps);
  return config;
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey clearBrowsingHistory];
  [HttpsUpgradeAppInterface clearAllowlist];

  if ([self testType] == TestType::kHttpsOnlyMode) {
    [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kHttpsOnlyModeEnabled];
  }
}

- (void)tearDown {
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kHttpsOnlyModeEnabled];
  [HttpsUpgradeAppInterface clearAllowlist];

  [super tearDown];
}

// Returns true if the HTTPS-Only Mode interstitial is enabled.
- (bool)isInterstitialEnabled {
  return [self testType] == TestType::kHttpsOnlyMode &&
         [ChromeEarlGrey userBooleanPref:prefs::kHttpsOnlyModeEnabled];
}

- (TestType)testType {
  return TestType::kHttpsOnlyMode;
}

// Asserts that the navigation wasn't upgraded.
- (void)assertNoUpgrade {
  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:0
                        forHistogram:@(security_interstitials::https_only_mode::
                                           kEventHistogram)],
                @"Shouldn't record event histogram");
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"Timer is still running");
  GREYAssert(![HttpsUpgradeAppInterface isOmniboxUpgradeTimerRunning],
             @"Omnibox upgrade timer is unexpectedly running");

  // Omnibox HTTPS Upgrades shouldn't handle this navigation.
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:0
              forHistogram:@(security_interstitials::omnibox_https_upgrades::
                                 kEventHistogram)],
      @"Omnibox HTTPS Upgrades unexpectedly recorded a histogram event");
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

  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"Timer is still running");
  GREYAssert(![HttpsUpgradeAppInterface isOmniboxUpgradeTimerRunning],
             @"Omnibox upgrade timer is unexpectedly running");

  // Omnibox HTTPS Upgrades shouldn't handle this navigation.
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:0
              forHistogram:@(security_interstitials::omnibox_https_upgrades::
                                 kEventHistogram)],
      @"Omnibox HTTPS Upgrades unexpectedly recorded a histogram event");
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
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"Timer is still running");
  GREYAssert(![HttpsUpgradeAppInterface isOmniboxUpgradeTimerRunning],
             @"Omnibox upgrade timer is unexpectedly running");

  // Omnibox HTTPS Upgrades shouldn't handle this navigation.
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:0
              forHistogram:@(security_interstitials::omnibox_https_upgrades::
                                 kEventHistogram)],
      @"Omnibox HTTPS Upgrades unexpectedly recorded a histogram event");
}

// Asserts that the metrics are properly recorded for a timed-out upgrade.
// repeatCount is the expected number of times the upgrade failed.
- (void)assertTimedOutUpgrade:(int)repeatCount {
  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:(repeatCount * 2)
                        forHistogram:@(security_interstitials::https_only_mode::
                                           kEventHistogram)],
                @"Incorrect number of records in event histogram");

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
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"Timer is still running");
  GREYAssert(![HttpsUpgradeAppInterface isOmniboxUpgradeTimerRunning],
             @"Omnibox upgrade timer is unexpectedly running");

  // Omnibox HTTPS Upgrades shouldn't handle this navigation.
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:0
              forHistogram:@(security_interstitials::omnibox_https_upgrades::
                                 kEventHistogram)],
      @"Omnibox HTTPS Upgrades unexpectedly recorded a histogram event");
}

#pragma mark - Tests

// Disable HTTPS-Only Mode and navigate to an HTTP URL directly.
// - If HTTPS Upgrades is disabled, this should load the HTTP URL even though
//   the upgraded HTTPS version serves good SSL.
// - Otherwise, it should load the HTTPS URL.
- (void)test_HttpsOnlyModeDisabled {
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kHttpsOnlyModeEnabled];

  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];

  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGrey waitForWebStateContainingText:"Revision"];

  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];

  if ([self testType] == TestType::kHttpsUpgrades) {
    [ChromeEarlGrey waitForWebStateContainingText:"HTTPS_RESPONSE"];
    [self assertSuccessfulUpgrade];
  } else {
    [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
    [self assertNoUpgrade];
  }
}

// Tests that navigations to localhost URLs aren't upgraded.
- (void)test_Localhost_ShouldNotUpgrade {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];

  GURL testURL = self.testServer->GetURL("/");
  GURL::Replacements replacements;
  replacements.SetHostStr("localhost");
  GURL localhostURL = testURL.ReplaceComponents(replacements);

  [ChromeEarlGrey loadURL:localhostURL];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  [self assertNoUpgrade];
}

// Navigate to an HTTPS URL directly. The navigation shouldn't be upgraded.
- (void)test_GoodHTTPS_ShouldNotUpgrade {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];

  GURL testURL = self.goodHTTPSServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTPS_RESPONSE"];
  [self assertNoUpgrade];
}

// Navigate to an HTTP URL directly. The upgraded HTTPS version serves good SSL.
// This should end up loading the HTTPS version of the URL.
- (void)test_HTTPWithGoodHTTPS_ShouldUpgrade {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];

  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTPS_RESPONSE"];
  [self assertSuccessfulUpgrade];
}

// Navigate to an HTTP URL by clicking a link. This should end up loading the
// HTTPS version of the URL.
- (void)test_HTTPWithGoodHTTPS_LinkClick_ShouldUpgrade {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];
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

// Navigate to an HTTP URL by posting a form. This should not be upgraded to
// HTTPS.
- (void)test_HTTPWithGoodHTTPS_Post_ShouldNotUpgrade {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];
  int HTTPPort = self.testServer->port();

  GURL testURL(base::StringPrintf(
      "data:text/html,"
      "<form method='POST' action='http://127.0.0.1:%d/good-https' id='myform'>"
      "<input name='test' value='test'><br>"
      "<input type='submit' id='submit-btn' value='Submit'></form><br>READY",
      HTTPPort));
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:"READY"];

  // Post the form. Should load the http URL.
  [ChromeEarlGrey tapWebStateElementWithID:@"submit-btn"];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  [self assertNoUpgrade];
}

// Navigate to an HTTP URL directly. The upgraded HTTPS version serves good SSL
// which redirects to the original HTTP URL. This should show the interstitial.
- (void)test_HTTPSRedirectsToHTTP_ShouldFallback {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.goodHTTPSServer->port()
                                      useFakeHTTPS:true];

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
  if ([self isInterstitialEnabled]) {
    [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
    [self assertFailedUpgrade:1];

    // Click through the interstitial. This should load the HTTP page.
    [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
  }

  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"Timer is still running");

  // Going back should go to chrome://version.
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:"Revision"];
  [self assertFailedUpgrade:1];
}

// Tests that prerendered navigations that should be upgraded are cancelled.
// This test is adapted from testTapPrerenderSuggestions() in
// prerender_egtest.mm.
// TODO(crbug.com/40833424): Reenable.
- (void)DISABLED_test_BadHTTPS_ShouldCancelPrerender {
  // TODO(crbug.com/40553918): Re-enable the test on iPad once the alternate
  // letters problem is fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(
        @"Disabled for iPad due to alternate letters educational screen.");
  }

  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.badHTTPSServer->port()
                                      useFakeHTTPS:false];

  [ChromeEarlGrey clearBrowsingHistory];

  // Type the full URL. This will show an interstitial. This adds the URL to
  // history.
  GURL testURL = self.testServer->GetURL("/");
  NSString* pageString = base::SysUTF8ToNSString(testURL.GetContent());
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(pageString)];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
  [self assertFailedUpgrade:1];
  GREYAssertEqual(2, _HTTPResponseCounter,
                  @"The server should have responded twice");

  // Click through the interstitial.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"Timer is still running");
  GREYAssertEqual(3, _HTTPResponseCounter,
                  @"The server should have responded three times");

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
      performAction:grey_replaceText([pageString substringToIndex:1])];

  // Wait until prerender request reaches the server.
  bool prerendered = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return self->_HTTPResponseCounter > 3;
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
  // TODO(crbug.com/40825375): Check that the CANCEL bucket has non-zero
  // elements. Not currently supported by MetricsAppInterface.
}

// Regression test for crbug.com/1379261. Checks that cancelling a prerendered
// navigation doesn't cause a crash. Steps are:
// 1. Disable HTTPS-Only Mode and visit an http:// URL. This puts the
//    URL in browser history.
// 2. Close tabs and reopen. Enable HTTPS-Only Mode.
// 3. Type the first letter of the http:// URL in step 1. This will prerender
//    the http URL.
// 4. Check that the prerender was cancelled properly.
// TODO(crbug.com/40833424): Reenable.
- (void)DISABLED_test_Prerender_CancelShouldNotCrash {
  // TODO(crbug.com/40553918): Re-enable the test on iPad once the alternate
  // letters problem is fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(
        @"Disabled for iPad due to alternate letters educational screen.");
  }

  // Step 1: Disable HTTPS-Only Mode and visit an http:// URL. This puts the
  // URL in browser history.
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kHttpsOnlyModeEnabled];
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.badHTTPSServer->port()
                                      useFakeHTTPS:false];
  [ChromeEarlGrey clearBrowsingHistory];

  GURL testURL = self.testServer->GetURL("/");
  NSString* testURLString = base::SysUTF8ToNSString(testURL.GetContent());
  [ChromeEarlGrey loadURL:testURL];
  GREYAssertEqual(1, _HTTPResponseCounter,
                  @"The server should have responded once");
  [ChromeEarlGrey goBack];

  // Step 2: Close tabs and reopen. Enable HTTPS-Only Mode.
  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kHttpsOnlyModeEnabled];

  // Step 3: Type the first letter of the http:// URL in step 1. This will
  // prerender the http URL.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText([testURLString substringToIndex:1])];

  bool prerendered = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    // The first response was for the http:// URL. The remaining responses are
    // prerendered. When the whole test suite runs, we may get more than one
    // prerendered navigation here.
    return self->_HTTPResponseCounter > 1;
  });
  GREYAssertTrue(prerendered, @"Prerender did not happen");

  // Step 4: Check that the prerender was cancelled properly.
  // Check the histograms. All prerender attempts must be cancelled. Relying on
  // the histogram here isn't great, but there doesn't seem to be a good
  // way of testing that prerenders have been cancelled.
  GREYAssertNil(
      [MetricsAppInterface expectCount:0
                             forBucket:/*PRERENDER_FINAL_STATUS_USED=*/0
                          forHistogram:@"Prerender.FinalStatus"],
      @"Prerender was used");
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"Timer is still running");
  GREYAssert(![HttpsUpgradeAppInterface isOmniboxUpgradeTimerRunning],
             @"Omnibox upgrade timer is unexpectedly running");

  // Check that the HTTPS-Only Mode tab helper recorded the prerender
  // cancellation.
  // First server response loaded normally, the rest should be prerenders.
  int prerenderCount = self->_HTTPResponseCounter - 1;
  GREYAssertNil([MetricsAppInterface
                    expectTotalCount:prerenderCount
                        forHistogram:@(security_interstitials::https_only_mode::
                                           kEventHistogram)],
                @"Failed to record event histogram");
  GREYAssertNil([MetricsAppInterface
                     expectCount:prerenderCount
                       forBucket:static_cast<int>(
                                     security_interstitials::https_only_mode::
                                         Event::kPrerenderCancelled)
                    forHistogram:@(security_interstitials::https_only_mode::
                                       kEventHistogram)],
                @"Failed to record prerender cancellation");
}

// Navigate to an HTTP URL and allowlist the URL. Then clear browsing data.
// This should clear the HTTP allowlist.
- (void)test_RemoveBrowsingData_ShouldClearAllowlist {
  if (![self isInterstitialEnabled]) {
    // Only relevant for HTTPS-Only Mode.
    // TODO(crbug.com/40914607): Enable for HTTPS-Upgrades when it implements
    // allowlisting.
    return;
  }
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.badHTTPSServer->port()
                                      useFakeHTTPS:false];

  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
  [self assertFailedUpgrade:1];

  // Click through the interstitial. This should load the HTTP page. Histogram
  // numbers shouldn't change.
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  [self assertFailedUpgrade:1];

  // Reload. Since the URL is now allowlisted, this should immediately load
  // HTTP without trying to upgrade. Histogram numbers shouldn't change.
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  [self assertFailedUpgrade:1];

  // Clear the allowlist by clearing the browsing data. This clears the history
  // programmatically, so it won't automatically reload the tabs.
  [ChromeEarlGrey clearBrowsingHistory];

  // Reloading the should show the interstitial again.
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
  [self assertFailedUpgrade:2];

  // Reload once more.
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
  [self assertFailedUpgrade:3];
}

// Click on the "Learn more" link in the interstitial. This should open a
// new tab.
- (void)test_ClickLearnMore_ShouldOpenNewTab {
  if (![self isInterstitialEnabled]) {
    // Only relevant for HTTPS-Only mode tests.
    return;
  }
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.badHTTPSServer->port()
                                      useFakeHTTPS:false];

  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
  [self assertFailedUpgrade:1];

  // Check tab count prior to tapping the link.
  NSUInteger oldRegularTabCount = [ChromeEarlGrey mainTabCount];
  NSUInteger oldIncognitoTabCount = [ChromeEarlGrey incognitoTabCount];

  [ChromeEarlGrey tapWebStateElementWithID:@"learn-more-link"];

  // A new tab should open after tapping the link.
  [ChromeEarlGrey waitForMainTabCount:oldRegularTabCount + 1];
  [ChromeEarlGrey waitForIncognitoTabCount:oldIncognitoTabCount];
}

// Navigate to an HTTP URL directly. The upgraded HTTPS version serves bad SSL.
// The upgrade will fail and the HTTPS-Only mode interstitial will be shown.
// Reloading the page should show the interstitial again.
- (void)test_BadHTTPS_ReloadInterstitial {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.badHTTPSServer->port()
                                      useFakeHTTPS:false];

  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  if ([self isInterstitialEnabled]) {
    [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
  } else {
    [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  }
  [self assertFailedUpgrade:1];

  [ChromeEarlGrey reload];
  if ([self isInterstitialEnabled]) {
    [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
  } else {
    [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  }
  [self assertFailedUpgrade:2];
}

// Navigate to an HTTP URL directly. The upgraded HTTPS version serves slow SSL.
// The upgrade will fail and the HTTPS-Only mode interstitial will be shown.
// Reloading the page should show the interstitial again.
- (void)test_SlowHTTPS_ReloadInterstitial {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.slowServer->port()
                                      useFakeHTTPS:true];
  // Set the fallback delay to zero. This will immediately stop the HTTPS
  // upgrade attempt.
  [HttpsUpgradeAppInterface setFallbackDelayForTesting:0];

  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  if ([self isInterstitialEnabled]) {
    [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
  } else {
    [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  }
  [self assertTimedOutUpgrade:1];

  [ChromeEarlGrey reload];
  if ([self isInterstitialEnabled]) {
    [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
  } else {
    [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  }
  [self assertTimedOutUpgrade:2];
}

// Navigate to an HTTP URL directly. The upgraded HTTPS version serves bad SSL.
// The upgrade will fail and the HTTPS-Only mode interstitial will be shown.
// Click through the interstitial, then reload the page. The HTTP page should
// be shown.
- (void)test_BadHTTPS_ProceedInterstitial_Allowlisted {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.badHTTPSServer->port()
                                      useFakeHTTPS:false];

  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  if ([self isInterstitialEnabled]) {
    [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
    // Click through the interstitial. This should load the HTTP page.
    [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
  }
  [self assertFailedUpgrade:1];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"Timer is still running");

  // Reload. Since the URL is now allowlisted, this should immediately load
  // HTTP without trying to upgrade.
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];

  if ([self isInterstitialEnabled]) {
    // If HTTPS-Only mode is enabled, clicking through the interstitial will
    // allowlist the site so no new histogram entry will be recorded.
    // Failed upgrades record two entries and this number shouldn't change.
    GREYAssertNil([MetricsAppInterface
                      expectTotalCount:2
                          forHistogram:@(security_interstitials::
                                             https_only_mode::kEventHistogram)],
                  @"Unexpected histogram event recorded.");
  } else {
    // HTTPS-Upgrades will attempt to upgrade and fail for the second time.
    [self assertFailedUpgrade:2];
  }
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"Timer is still running");

  // Open a new tab and go to the same URL. Should load the page without an
  // interstitial.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"Timer is still running");
  if ([self isInterstitialEnabled]) {
    [self assertFailedUpgrade:1];
  } else {
    // HTTPS-Upgrades will attempt to upgrade and fail for the third time.
    [self assertFailedUpgrade:3];
  }

  // Open an incognito tab and try there. Should show the interstitial as
  // allowlist decisions don't carry over to incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  // Set the testing information for the incognito tab.
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.badHTTPSServer->port()
                                      useFakeHTTPS:false];
  [ChromeEarlGrey loadURL:testURL];

  if ([self isInterstitialEnabled]) {
    [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
    // Click through the interstitial. This should load the HTTP page.
    [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
    [self assertFailedUpgrade:2];
  } else {
    // HTTPS-Upgrades will attempt to upgrade and fail for the fourth time.
    [self assertFailedUpgrade:4];
  }
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"Timer is still running");

  [ChromeEarlGreyUI reload];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  if ([self isInterstitialEnabled]) {
    // If HTTPS-Only mode is enabled, this should immediately load HTTP without
    // trying to upgrade because it was allowlisted.
    [self assertFailedUpgrade:2];
  } else {
    // HTTPS-Upgrades will attempt to upgrade and fail for the fifth time.
    [self assertFailedUpgrade:5];
  }
}

// Same as testUpgrade_BadHTTPS_ProceedInterstitial_Allowlisted but uses
// a slow HTTPS response instead:
// Navigate to an HTTP URL directly. The upgraded HTTPS version serves a slow
// loading SSL page. The upgrade will be cancelled and the HTTPS-Only mode
// interstitial will be shown. Click through the interstitial, then reload the
// page. The HTTP page should be shown.
- (void)test_SlowHTTPS_ProceedInterstitial_Allowlisted {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.slowServer->port()
                                      useFakeHTTPS:true];
  // Set the fallback delay to zero. This will immediately stop the HTTPS
  // upgrade attempt.
  [HttpsUpgradeAppInterface setFallbackDelayForTesting:0];

  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  if ([self isInterstitialEnabled]) {
    [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
    // Click through the interstitial. This should load the HTTP page.
    [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
  }
  [self assertTimedOutUpgrade:1];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"Timer is still running");

  // Reload.
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  if ([self isInterstitialEnabled]) {
    // If HTTPS-Only mode is enabled, clicking through the interstitial will
    // allowlist the site so no new histogram entry will be recorded.
    // Failed upgrades record two entries and this number shouldn't change.
    GREYAssertNil([MetricsAppInterface
                      expectTotalCount:2
                          forHistogram:@(security_interstitials::
                                             https_only_mode::kEventHistogram)],
                  @"Unexpected histogram event recorded.");
  } else {
    // HTTPS-Upgrades will attempt to upgrade and fail for the second time.
    [self assertTimedOutUpgrade:2];
  }
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
             @"Timer is still running");
}

// Navigate to an HTTP URL directly. The upgraded HTTPS version serves bad SSL.
// The upgrade will fail and the HTTPS-Only mode interstitial will be shown.
// Tap Go back on the interstitial.
- (void)test_BadHTTPS_GoBack {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.badHTTPSServer->port()
                                      useFakeHTTPS:false];

  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGrey waitForWebStateContainingText:"Revision"];

  // Load a site with a bad HTTPS upgrade. This shows an interstitial.
  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  if ([self isInterstitialEnabled]) {
    [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
  } else {
    [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  }
  [self assertFailedUpgrade:1];

  if ([self isInterstitialEnabled]) {
    // Tap "Go back" on the interstitial. This should go back to
    // chrome://version.
    [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  } else {
    [ChromeEarlGrey goBack];
  }
  [ChromeEarlGrey waitForWebStateContainingText:"Revision"];

  // Go forward. Should hit the interstitial again.
  [ChromeEarlGrey goForward];
  if ([self isInterstitialEnabled]) {
    [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
    [self assertFailedUpgrade:2];
  } else {
    [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
    // TODO(crbug.com/40914607): This should equal to 2 instead.
    [self assertFailedUpgrade:1];
  }
}

// Same as testUpgrade_BadHTTPS_GoBack but uses a slow HTTPS response instead:
// Navigate to an HTTP URL directly. The upgraded HTTPS version serves a slow
// loading HTTPS page. The upgrade will be cancelled and the HTTPS-Only mode
// interstitial will be shown. Tap Go back on the interstitial.
- (void)test_SlowHTTPS_GoBack {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.slowServer->port()
                                      useFakeHTTPS:true];
  // Set the fallback delay to zero. This will immediately stop the HTTPS
  // upgrade attempt.
  [HttpsUpgradeAppInterface setFallbackDelayForTesting:0];

  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGrey waitForWebStateContainingText:"Revision"];

  // Load a site with a slow HTTPS upgrade. This shows an interstitial.
  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];
  if ([self isInterstitialEnabled]) {
    [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
  } else {
    [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  }
  [self assertTimedOutUpgrade:1];

  if ([self isInterstitialEnabled]) {
    // Tap "Go back" on the interstitial. This should go back to
    // chrome://version.
    [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  } else {
    [ChromeEarlGrey goBack];
  }
  [ChromeEarlGrey waitForWebStateContainingText:"Revision"];

  // Go forward. Should hit the interstitial again.
  [ChromeEarlGrey goForward];
  if ([self isInterstitialEnabled]) {
    [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
    [self assertTimedOutUpgrade:2];
  } else {
    [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
    // TODO(crbug.com/40914607): This should equal to 2 instead.
    [self assertTimedOutUpgrade:1];
  }
}

// Navigate to an HTTP URL and click through the interstitial. Then,
// navigate to a new page and go back. This should load the HTTP URL
// without showing the interstitial again.
- (void)test_BadHTTPS_GoBackToAllowlistedSite {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.badHTTPSServer->port()
                                      useFakeHTTPS:false];

  [ChromeEarlGrey loadURL:GURL("about:blank")];

  // Load a site with a bad HTTPS upgrade. This shows an interstitial.
  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];

  if ([self isInterstitialEnabled]) {
    [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
    [self assertFailedUpgrade:1];
    // Click through the interstitial. This should load the HTTP page.
    [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
  }

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
- (void)test_SlowHTTPS_GoBackToAllowlistedSite {
  [HttpsUpgradeAppInterface setHTTPSPortForTesting:self.slowServer->port()
                                      useFakeHTTPS:true];
  // Set the fallback delay to zero. This will immediately stop the HTTPS
  // upgrade attempt.
  [HttpsUpgradeAppInterface setFallbackDelayForTesting:0];

  [ChromeEarlGrey loadURL:GURL("about:blank")];

  // Load a site with a bad HTTPS upgrade. This shows an interstitial.
  GURL testURL = self.testServer->GetURL("/");
  [ChromeEarlGrey loadURL:testURL];

  if ([self isInterstitialEnabled]) {
    [ChromeEarlGrey waitForWebStateContainingText:kInterstitialText];
    [self assertTimedOutUpgrade:1];
    // Click through the interstitial. This should load the HTTP page.
    [ChromeEarlGrey tapWebStateElementWithID:@"proceed-button"];
  }

  [ChromeEarlGrey waitForWebStateContainingText:"HTTP_RESPONSE"];
  GREYAssert(![HttpsUpgradeAppInterface isHttpsOnlyModeTimerRunning],
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

// Tests for HTTPS-Upgrades feature.
// TODO(crbug.com/40849153): Remove the "ZZZ" when the bug is fixed.
@interface ZZZ_HttpsUpgradesTestCase : ZZZ_HttpsOnlyModeTestCase
@end

@implementation ZZZ_HttpsUpgradesTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_disabled = {omnibox::kDefaultTypedNavigationsToHttps};

  config.features_enabled.push_back(
      security_interstitials::features::kHttpsUpgrades);
  return config;
}

// This is currently needed to prevent this test case from being ignored.
- (void)testEmpty {
}

- (TestType)testType {
  return TestType::kHttpsUpgrades;
}

@end
