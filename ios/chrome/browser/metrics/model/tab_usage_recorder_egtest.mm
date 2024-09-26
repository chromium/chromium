// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/functional/bind.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/metrics/model/tab_usage_recorder_metrics.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "ios/web/public/test/http_server/delayed_response_provider.h"
#import "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using chrome_test_util::OpenLinkInNewTabButton;
using chrome_test_util::SettingsDestinationButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuPrivacyButton;
using chrome_test_util::TabGridCellAtIndex;
using chrome_test_util::WebViewMatcher;

namespace {

const char kTestUrl1[] =
    "http://ios/testing/data/http_server_files/memory_usage.html";
const char kURL1FirstWord[] = "Page";
const char kTestUrl2[] =
    "http://ios/testing/data/http_server_files/fullscreen.html";
const char kURL2FirstWord[] = "Rugby";
NSString* const kClearPageScript = @"document.body.innerHTML='';";

// The delay to use to serve slow URLs.
constexpr base::TimeDelta kSlowURLDelay = base::Seconds(3);

// The delay to use to wait for pate starting loading.
constexpr base::TimeDelta kWaitForPageLoadTimeout = base::Seconds(3);

// The delay to use to serve very slow URLS -- tests using this delay expect the
// page to never load.
constexpr base::TimeDelta kVerySlowURLDelay = base::Seconds(20);

// The delay to wait for an element to appear before tapping on it.
constexpr base::TimeDelta kWaitElementTimeout = base::Seconds(3);

// Wait until `matcher` is accessible (not nil).
void Wait(id<GREYMatcher> matcher, NSString* name) {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_notNil()
                                                             error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(kWaitElementTimeout,
                                                          condition),
             @"Waiting for matcher %@ failed.", name);
}

// Creates a new main tab and load `url`. Wait until `word` is visible on the
// page.
void NewMainTabWithURL(const GURL& url, const std::string& word) {
  int number_of_tabs = [ChromeEarlGrey mainTabCount];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:word];
  [ChromeEarlGrey waitForMainTabCount:(number_of_tabs + 1)];
}

// Opens 2 new tabs with different URLs.
void OpenTwoTabs() {
  [ChromeEarlGrey closeAllTabsInCurrentMode];
  const GURL url1 = web::test::HttpServer::MakeUrl(kTestUrl1);
  const GURL url2 = web::test::HttpServer::MakeUrl(kTestUrl2);
  NewMainTabWithURL(url1, kURL1FirstWord);
  NewMainTabWithURL(url2, kURL2FirstWord);
}

// Closes a tab in the current tab model. Synchronize on tab number afterwards.
void CloseTabAtIndexAndSync(NSUInteger i) {
  NSUInteger nb_main_tab = [ChromeEarlGrey mainTabCount];
  [ChromeEarlGrey closeTabAtIndex:i];
  ConditionBlock condition = ^{
    return [ChromeEarlGrey mainTabCount] == (nb_main_tab - 1);
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(kWaitElementTimeout,
                                                          condition),
             @"Waiting for tab to close");
}

void SwitchToNormalMode() {
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Switching to normal mode is only allowed from Incognito.");

  // Enter the tab grid to switch modes.
  [ChromeEarlGrey showTabSwitcher];

  // Switch modes and exit the tab grid.
  const int tab_index = [ChromeEarlGrey indexOfActiveNormalTab];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:TabGridCellAtIndex(tab_index)]
      performAction:grey_tap()];

  BOOL success = NO;
  // Turn off synchronization of GREYAssert to test the pending states.
  {
    ScopedSynchronizationDisabler disabler;
    success =
        base::test::ios::WaitUntilConditionOrTimeout(kWaitElementTimeout, ^{
          return ![ChromeEarlGrey isIncognitoMode];
        });
  }

  if (!success) {
    // TODO(crbug.com/40622599): Avoid asserting directly unless the test fails,
    // due to timing issues.
    GREYFail(@"Failed to switch to normal mode.");
  }
}

}  // namespace

// Test for the TabUsageRecorder class.
@interface TabUsageRecorderTestCase : WebHttpServerChromeTestCase
@end

@implementation TabUsageRecorderTestCase

- (void)setUp {
  [super setUp];
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  [ChromeEarlGrey removeBrowsingCache];
}

- (void)tearDown {
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
  [super tearDown];
}

// Tests that the recorder actual recorde tab state.
// TODO(crbug.com/41442581) The test is flaky.
- (void)DISABLED_testTabSwitchRecorder {
  [ChromeEarlGrey resetTabUsageRecorder];

  // Open two tabs with urls.
  OpenTwoTabs();
  [ChromeEarlGrey waitForMainTabCount:2];
  // Switch between the two tabs.  Both are currently in memory.
  [ChromeEarlGrey selectTabAtIndex:0];

  // Verify that one in-memory tab switch has been recorded.
  // histogramTester.ExpectTotalCount(kSelectedTabHistogramName, 1,
  // failureBlock);
  NSError* error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@(tab_usage_recorder::kSelectedTabHistogramName)];
  if (error) {
    GREYFail([error description]);
  }
  error = [MetricsAppInterface
      expectUniqueSampleWithCount:1
                        forBucket:tab_usage_recorder::IN_MEMORY
                     forHistogram:
                         @(tab_usage_recorder::kSelectedTabHistogramName)];
  if (error) {
    GREYFail([error description]);
  }

  // Evict the tab.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey evictOtherBrowserTabs];

  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Failed to switch to incognito mode");

  // Switch back to the normal tabs. Should be on tab one.
  SwitchToNormalMode();

  [ChromeEarlGrey waitForWebStateContainingText:kURL1FirstWord];

  error = [MetricsAppInterface
      expectTotalCount:2
          forHistogram:@(tab_usage_recorder::kSelectedTabHistogramName)];
  if (error) {
    GREYFail([error description]);
  }
  error = [MetricsAppInterface
       expectCount:1
         forBucket:tab_usage_recorder::EVICTED
      forHistogram:@(tab_usage_recorder::kSelectedTabHistogramName)];
  if (error) {
    GREYFail([error description]);
  }
}

// Verifies the UMA metric for page loads before a tab eviction by loading
// some tabs, forcing a tab eviction, then checking the histogram.
- (void)testPageLoadCountBeforeEvictedTab {
  [ChromeEarlGrey resetTabUsageRecorder];
  const GURL url1 = web::test::HttpServer::MakeUrl(kTestUrl1);
  // This test opens three tabs.
  const int numberOfTabs = 3;
  [ChromeEarlGrey closeAllTabsInCurrentMode];

  // Open three tabs with http:// urls.
  for (NSUInteger i = 0; i < numberOfTabs; i++) {
    [ChromeEarlGrey openNewTab];
    [ChromeEarlGrey loadURL:url1];
    [ChromeEarlGrey waitForWebStateContainingText:kURL1FirstWord];
    [ChromeEarlGrey waitForMainTabCount:(i + 1)];
  }

  // Switch between the tabs. They are currently in memory.
  [ChromeEarlGrey selectTabAtIndex:0];

  // Verify that no page-load count has been recorded.
  NSError* error = [MetricsAppInterface
      expectTotalCount:0
          forHistogram:
              @(tab_usage_recorder::kPageLoadsBeforeEvictedTabSelected)];

  if (error) {
    GREYFail([error description]);
  }

  // Reload each tab.
  for (NSUInteger i = 0; i < numberOfTabs; i++) {
    [ChromeEarlGrey selectTabAtIndex:i];
    // Clear the page so that we can check when page reload is complete.
    [ChromeEarlGrey evaluateJavaScriptForSideEffect:kClearPageScript];

    [ChromeEarlGrey reload];
    [ChromeEarlGrey waitForWebStateContainingText:kURL1FirstWord];
  }

  // Evict the tab. Create a dummy tab so that switching back to normal mode
  // does not trigger a reload immediately.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey evictOtherBrowserTabs];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  // Switch back to the normal tabs. Should be on tab one.
  SwitchToNormalMode();

  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:kURL1FirstWord];

  // Verify that one page-load count has been recorded. It should contain two
  // page loads for each tab created.
  error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:
              @(tab_usage_recorder::kPageLoadsBeforeEvictedTabSelected)];

  if (error) {
    GREYFail([error description]);
  }

  error = [MetricsAppInterface
         expectSum:numberOfTabs * 2
      forHistogram:@(tab_usage_recorder::kPageLoadsBeforeEvictedTabSelected)];
  if (error) {
    GREYFail([error description]);
  }
}

// Tests that tabs reloaded on cold start are reported as
// EVICTED_DUE_TO_COLD_START.
// TODO(crbug.com/41442581) The test is disabled due to flakiness.
- (void)DISABLED_testColdLaunchReloadCount {
  [ChromeEarlGrey resetTabUsageRecorder];

  // Open two tabs with urls.
  OpenTwoTabs();
  [ChromeEarlGrey waitForMainTabCount:2];

  // Set the normal tabs as 'cold start' tabs.
  [ChromeEarlGrey setCurrentTabsToBeColdStartTabs];

  // Open two incognito tabs with urls, clearing normal tabs from memory.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey evictOtherBrowserTabs];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey evictOtherBrowserTabs];

  [ChromeEarlGrey waitForIncognitoTabCount:2];

  // Switch back to the normal tabs.
  SwitchToNormalMode();
  {
    ScopedSynchronizationDisabler disabler;
    Wait(chrome_test_util::ToolsMenuButton(), @"Tool Menu");
  }
  [ChromeEarlGrey waitForWebStateContainingText:kURL2FirstWord];

  // Select the other one so it also reloads.
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:kURL1FirstWord];

  // Make sure that one of the 2 tab loads (excluding the selected tab) is
  // counted as a cold start eviction.
  NSError* error = [MetricsAppInterface
       expectCount:1
         forBucket:tab_usage_recorder::EVICTED_DUE_TO_COLD_START
      forHistogram:@(tab_usage_recorder::kSelectedTabHistogramName)];
  if (error) {
    GREYFail([error description]);
  }
  error = [MetricsAppInterface
       expectCount:0
         forBucket:tab_usage_recorder::IN_MEMORY
      forHistogram:@(tab_usage_recorder::kSelectedTabHistogramName)];
  if (error) {
    GREYFail([error description]);
  }

  // Re-select the same tab and make sure it is not counted again as evicted.
  [ChromeEarlGrey selectTabAtIndex:1];
  [ChromeEarlGrey selectTabAtIndex:0];

  [ChromeEarlGrey waitForWebStateContainingText:kURL1FirstWord];
  error = [MetricsAppInterface
       expectCount:1
         forBucket:tab_usage_recorder::EVICTED_DUE_TO_COLD_START
      forHistogram:@(tab_usage_recorder::kSelectedTabHistogramName)];
  if (error) {
    GREYFail([error description]);
  }
  error = [MetricsAppInterface
       expectCount:2
         forBucket:tab_usage_recorder::IN_MEMORY
      forHistogram:@(tab_usage_recorder::kSelectedTabHistogramName)];
  if (error) {
    GREYFail([error description]);
  }
}

// Tests that tabs reloads after backgrounding and eviction.
// TODO(crbug.com/41442581) The test is flaky.
- (void)DISABLED_testBackgroundingReloadCount {
  [ChromeEarlGrey resetTabUsageRecorder];

  // Open two tabs with urls.
  OpenTwoTabs();
  [ChromeEarlGrey waitForMainTabCount:2];

  // Simulate going into the background.
  [ChromeEarlGrey simulateTabsBackgrounding];

  // Open incognito and clear normal tabs from memory.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey evictOtherBrowserTabs];
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Failed to switch to incognito mode");

  // Switch back to the normal tabs.
  SwitchToNormalMode();
  // Wait for the page starting to load. It is possible that the page finish
  // loading before this test. In that case the wait will timeout. Ignore the
  // result.
  bool unused =
      base::test::ios::WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
        return [ChromeEarlGrey isLoading];
      });
  (void)unused;

  [ChromeEarlGrey waitForWebStateContainingText:kURL2FirstWord];

  const GURL url1 = web::test::HttpServer::MakeUrl(kTestUrl1);
  const GURL url2 = web::test::HttpServer::MakeUrl(kTestUrl2);
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(url2.GetContent())]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:kURL1FirstWord];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(url1.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Test that USER_DID_NOT_WAIT is reported if the user does not wait for the
// reload to be complete after eviction.
- (void)testEvictedTabSlowReload {
  std::map<GURL, std::string> responses;
  const GURL slowURL = web::test::HttpServer::MakeUrl("http://slow");
  responses[slowURL] = "Slow Page";
  web::test::SetUpHttpServer(std::make_unique<web::DelayedResponseProvider>(
      std::make_unique<HtmlResponseProvider>(responses), kSlowURLDelay));

  // A blank tab needed to switch to it after reloading.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:slowURL waitForCompletion:YES];
  // Wait for the page starting to load. It is possible that the page finish
  // loading before this test. In that case the wait will timeout. Ignore the
  // result.
  bool unused =
      base::test::ios::WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
        return [ChromeEarlGrey isLoading];
      });
  (void)unused;
  [ChromeEarlGrey waitForPageToFinishLoading];

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey evictOtherBrowserTabs];
  [ChromeEarlGrey removeBrowsingCache];

  SwitchToNormalMode();
  // TODO(crbug.com/41271925): EarlGrey synchronize on some animations when a
  // page is loading. Need to handle synchronization manually for this test.
  {
    ScopedSynchronizationDisabler disabler;
    Wait(chrome_test_util::ToolsMenuButton(), @"Tool Menu");
    // This method is not synced on EarlGrey.
    [ChromeEarlGrey selectTabAtIndex:0];
    // Wait for the page starting to load. It is possible that the page finish
    // loading before this test. In that case the wait will timeout. Ignore the
    // result.
    unused =
        base::test::ios::WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
          return [ChromeEarlGrey isLoading];
        });
    (void)unused;
  }
}

// Test that the USER_DID_NOT_WAIT metric is logged when the user opens an NTP
// while the evicted tab is still reloading.
- (void)testEvictedTabReloadSwitchToNTP {
  std::map<GURL, std::string> responses;
  const GURL slowURL = web::test::HttpServer::MakeUrl("http://slow");
  responses[slowURL] = "Slow Page";

  web::test::SetUpHttpServer(std::make_unique<web::DelayedResponseProvider>(
      std::make_unique<HtmlResponseProvider>(responses), kSlowURLDelay));

  NewMainTabWithURL(slowURL, "Slow");
  // Wait for the page starting to load. It is possible that the page finish
  // loading before this test. In that case the wait will timeout. Ignore the
  // result.
  bool unused =
      base::test::ios::WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
        return [ChromeEarlGrey isLoading];
      });
  (void)unused;
  [ChromeEarlGrey waitForPageToFinishLoading];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey evictOtherBrowserTabs];

  [ChromeEarlGrey removeBrowsingCache];

  SwitchToNormalMode();
  // Wait for the page starting to load. It is possible that the page finish
  // loading before this test. In that case the wait will timeout. Ignore the
  // result.
  unused =
      base::test::ios::WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
        return [ChromeEarlGrey isLoading];
      });
  (void)unused;
  // TODO(crbug.com/41271925): EarlGrey synchronize on some animations when a
  // page is loading. Need to handle synchronization manually for this test.
  {
    ScopedSynchronizationDisabler disabler;
    Wait(chrome_test_util::ToolsMenuButton(), @"Tool Menu");
  }

  [ChromeEarlGrey openNewTab];
}

// Test that the USER_DID_NOT_WAIT metric is not logged when the user opens
// and closes the settings UI while the evicted tab is still reloading.
// TODO(crbug.com/369787152): The test is flaky on simulator.
#if TARGET_OS_SIMULATOR
#define MAYBE_testEvictedTabReloadSettingsAndBack \
  FLAKY_testEvictedTabReloadSettingsAndBack
#else
#define MAYBE_testEvictedTabReloadSettingsAndBack \
  testEvictedTabReloadSettingsAndBack
#endif
- (void)MAYBE_testEvictedTabReloadSettingsAndBack {
  std::map<GURL, std::string> responses;
  const GURL slowURL = web::test::HttpServer::MakeUrl("http://slow");
  responses[slowURL] = "Slow Page";

  web::test::SetUpHttpServer(std::make_unique<web::DelayedResponseProvider>(
      std::make_unique<HtmlResponseProvider>(responses), kSlowURLDelay));
  NewMainTabWithURL(slowURL, responses[slowURL]);
  // Wait for the page starting to load. It is possible that the page finish
  // loading before this test. In that case the wait will timeout. Ignore the
  // result.
  bool unused =
      base::test::ios::WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
        return [ChromeEarlGrey isLoading];
      });
  (void)unused;
  [ChromeEarlGrey waitForPageToFinishLoading];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey evictOtherBrowserTabs];

  [ChromeEarlGrey removeBrowsingCache];
  SwitchToNormalMode();
  // Wait for the page starting to load. It is possible that the page finish
  // loading before this test. In that case the wait will timeout. Ignore the
  // result.
  unused =
      base::test::ios::WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
        return [ChromeEarlGrey isLoading];
      });
  (void)unused;
  [ChromeEarlGreyUI openSettingsMenu];

  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:responses[slowURL]];
}

// Tests that leaving Chrome while an evicted tab is reloading triggers the
// recording of the USER_LEFT_CHROME metric.
- (void)testEvictedTabReloadBackgrounded {
  std::map<GURL, std::string> responses;
  const GURL slowURL = web::test::HttpServer::MakeUrl("http://slow");
  responses[slowURL] = "Slow Page";

  web::test::SetUpHttpServer(std::make_unique<HtmlResponseProvider>(responses));

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:slowURL waitForCompletion:YES];
  // Wait for the page starting to load. It is possible that the page finish
  // loading before this test. In that case the wait will timeout. Ignore the
  // result.
  bool unused =
      base::test::ios::WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
        return [ChromeEarlGrey isLoading];
      });
  (void)unused;
  [ChromeEarlGrey waitForPageToFinishLoading];

  int nb_incognito_tab = [ChromeEarlGrey incognitoTabCount];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:(nb_incognito_tab + 1)];
  [ChromeEarlGrey evictOtherBrowserTabs];
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kWaitElementTimeout,
                 ^{
                   return [ChromeEarlGrey isIncognitoMode];
                 }),
             @"Fail to switch to incognito mode.");

  web::test::SetUpHttpServer(std::make_unique<web::DelayedResponseProvider>(
      std::make_unique<HtmlResponseProvider>(responses), kVerySlowURLDelay));

  [ChromeEarlGrey removeBrowsingCache];

  SwitchToNormalMode();
  // Wait for the page starting to load. It is possible that the page finish
  // loading before this test. In that case the wait will timeout. Ignore the
  // result.
  unused =
      base::test::ios::WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
        return [ChromeEarlGrey isLoading];
      });
  (void)unused;

  {
    ScopedSynchronizationDisabler disabler;
    Wait(chrome_test_util::ToolsMenuButton(), @"Tool Menu");

    [ChromeEarlGrey simulateTabsBackgrounding];
  }
}

// Tests that redirecting pages are not reloaded after eviction.
- (void)testPageRedirect {
  GURL redirectURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/redirect_refresh.html");
  GURL destinationURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  [ChromeEarlGrey resetTabUsageRecorder];

  NewMainTabWithURL(redirectURL, "arrived");

  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  NSUInteger tabIndex = [ChromeEarlGrey mainTabCount] - 1;
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey evictOtherBrowserTabs];

  SwitchToNormalMode();

  [ChromeEarlGrey selectTabAtIndex:tabIndex];
  [ChromeEarlGrey waitForWebStateContainingText:"arrived"];

  // Verify that one page-load count has been recorded.  It should contain a
  // sum of 1 - one sample with 1 page load.
  NSError* error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:
              @(tab_usage_recorder::kPageLoadsBeforeEvictedTabSelected)];
  if (error) {
    GREYFail([error description]);
  }

  error = [MetricsAppInterface
         expectSum:1
      forHistogram:@(tab_usage_recorder::kPageLoadsBeforeEvictedTabSelected)];
  if (error) {
    GREYFail([error description]);
  }
}

// Tests that navigations are correctly reported in
// Tab.PageLoadsSinceLastSwitchToEvictedTab histogram.
- (void)testLinkClickNavigation {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL initialURL =
      web::test::HttpServer::MakeUrl("http://scenarioTestLinkClickNavigation");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://destination");
  responses[initialURL] = base::StringPrintf(
      "<body><a style='margin-left:50px' href='%s' id='link'>link</a></body>",
      destinationURL.spec().c_str());
  responses[destinationURL] = "Whee!";
  web::test::SetUpHttpServer(std::make_unique<HtmlResponseProvider>(responses));
  [ChromeEarlGrey resetTabUsageRecorder];

  // Open a tab with a link to click.
  NewMainTabWithURL(initialURL, "link");
  // Click the link.
  [ChromeEarlGrey tapWebStateElementWithID:@"link"];

  [ChromeEarlGrey waitForWebStateContainingText:"Whee"];
  NSUInteger tabIndex = [ChromeEarlGrey mainTabCount] - 1;

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey evictOtherBrowserTabs];
  SwitchToNormalMode();

  [ChromeEarlGrey selectTabAtIndex:tabIndex];
  // Wait for the page starting to load. It is possible that the page finish
  // loading before this test. In that case the wait will timeout. Ignore the
  // result.
  bool unused =
      base::test::ios::WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
        return [ChromeEarlGrey isLoading];
      });
  (void)unused;
  [ChromeEarlGrey waitForWebStateContainingText:"Whee"];

  // Verify that the page-load count has been recorded.  It should contain a
  // sum of 2 - one sample with 2 page loads.
  NSError* error = [MetricsAppInterface
         expectSum:2
      forHistogram:@(tab_usage_recorder::kPageLoadsBeforeEvictedTabSelected)];
  if (error) {
    GREYFail([error description]);
  }

  // Verify that only one evicted tab was selected.  This is to make sure the
  // link click did not generate an evicted-tab-reload count.
  error = [MetricsAppInterface
       expectCount:1
         forBucket:tab_usage_recorder::EVICTED
      forHistogram:@(tab_usage_recorder::kSelectedTabHistogramName)];
  if (error) {
    GREYFail([error description]);
  }
}

// Tests that opening links in a new tab will not evict the source tab.
- (void)testOpenLinkInNewTab {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL initialURL =
      web::test::HttpServer::MakeUrl("http://scenarioTestOpenLinkInNewTab");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://destination");
  // Make the link that cover the whole page so that long pressing the web view
  // will trigger the link context menu.
  responses[initialURL] =
      base::StringPrintf("<body style='width:auto; height:auto;'><a href='%s' "
                         "id='link'><div style='width:100%%; "
                         "height:100%%;'>link</div></a></body>",
                         destinationURL.spec().c_str());
  responses[destinationURL] = "Whee!";
  web::test::SetUpHttpServer(std::make_unique<HtmlResponseProvider>(responses));
  [ChromeEarlGrey resetTabUsageRecorder];

  // Open a tab with a link to click.
  NewMainTabWithURL(initialURL, "link");

  int numberOfTabs = [ChromeEarlGrey mainTabCount];
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:"link"],
                        true /* menu should appear */)];

  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForMainTabCount:(numberOfTabs + 1)];

  [ChromeEarlGrey selectTabAtIndex:numberOfTabs];

  [ChromeEarlGreyUI waitForAppToIdle];
  [ChromeEarlGrey waitForWebStateContainingText:"Whee"];

  NSError* error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@(tab_usage_recorder::kSelectedTabHistogramName)];
  if (error) {
    GREYFail([error description]);
  }
  error = [MetricsAppInterface
       expectCount:1
         forBucket:tab_usage_recorder::IN_MEMORY
      forHistogram:@(tab_usage_recorder::kSelectedTabHistogramName)];
  if (error) {
    GREYFail([error description]);
  }
}

// Tests that opening tabs from external app will not cause tab eviction.
- (void)testOpenFromApp {
  [ChromeEarlGrey resetTabUsageRecorder];

  [ChromeEarlGrey openNewTab];
  GURL url(kTestUrl1);

  [ChromeEarlGrey openURLFromExternalApp:url];

  // Add a delay to ensure the tab has fully opened.  Because the check below
  // is for zero metrics recorded, it adds no flakiness.  However, this pause
  // makes the step more likely to fail in failure cases.  I.e. without it, this
  // test would sometimes pass even when it should fail.
  base::test::ios::SpinRunLoopWithMaxDelay(base::Milliseconds(500));

  // Verify that zero Tab.StatusWhenSwitchedBackToForeground metrics were
  // recorded.  Tabs created at the time the user switches to them should not
  // be counted in this metric.
  NSError* error = [MetricsAppInterface
      expectTotalCount:0
          forHistogram:@(tab_usage_recorder::kSelectedTabHistogramName)];
  if (error) {
    GREYFail([error description]);
  }
}

// Verify that evicted tabs that are deleted are removed from the evicted tabs
// map.
- (void)testTabDeletion {
  [ChromeEarlGrey resetTabUsageRecorder];
  // Add an autorelease pool to delete the closed tabs before the end of the
  // test.
  @autoreleasepool {
    // Open two tabs with urls.
    OpenTwoTabs();
    // Set the normal tabs as 'cold start' tabs.
    [ChromeEarlGrey setCurrentTabsToBeColdStartTabs];
    // One more tab.
    const GURL url1 = web::test::HttpServer::MakeUrl(kTestUrl1);
    NewMainTabWithURL(url1, kURL1FirstWord);

    GREYAssertEqual([ChromeEarlGrey mainTabCount], 3,
                    @"Check number of normal tabs");
    // The cold start tab which was not active will still be evicted.
    GREYAssertEqual([ChromeEarlGrey evictedMainTabCount], 1,
                    @"Check number of evicted tabs");

    // Close two of the three open tabs without selecting them first.
    // This should delete the tab objects, even though they're still being
    // tracked by the tab usage recorder in its `evicted_tabs_` map.
    CloseTabAtIndexAndSync(1);

    GREYAssertEqual([ChromeEarlGrey mainTabCount], 2,
                    @"Check number of normal tabs");
    CloseTabAtIndexAndSync(0);
    GREYAssertEqual([ChromeEarlGrey mainTabCount], 1,
                    @"Check number of normal tabs");
    [ChromeEarlGreyUI waitForAppToIdle];
  }
  // The deleted tabs are purged during foregrounding and backgrounding.
  [ChromeEarlGrey simulateTabsBackgrounding];
  // Make sure `evicted_tabs_` purged the deleted tabs.
  int evicted = [ChromeEarlGrey evictedMainTabCount];
  GREYAssertEqual(evicted, 0, @"Check number of evicted tabs");
}

@end
