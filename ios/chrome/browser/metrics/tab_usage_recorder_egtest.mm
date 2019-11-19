// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include <memory>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder.h"
#import "ios/chrome/browser/metrics/tab_usage_recorder_test_util.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/test/app/browsing_data_test_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/histogram_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#include "ios/web/public/test/http_server/delayed_response_provider.h"
#include "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::OpenLinkInNewTabButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuButton;
using chrome_test_util::SettingsMenuPrivacyButton;
using chrome_test_util::WebViewMatcher;

using tab_usage_recorder_test_util::OpenNewIncognitoTabUsingUIAndEvictMainTabs;
using tab_usage_recorder_test_util::SwitchToNormalMode;

namespace {

const char kTestUrl1[] =
    "http://ios/testing/data/http_server_files/memory_usage.html";
const char kURL1FirstWord[] = "Page";
const char kTestUrl2[] =
    "http://ios/testing/data/http_server_files/fullscreen.html";
const char kURL2FirstWord[] = "Rugby";
const char kClearPageScript[] = "document.body.innerHTML='';";

// The delay to use to serve slow URLs.
const CGFloat kSlowURLDelay = 3;

// The delay to use to serve very slow URLS -- tests using this delay expect the
// page to never load.
const CGFloat kVerySlowURLDelay = 20;

// The delay to wait for an element to appear before tapping on it.
const CGFloat kWaitElementTimeout = 3;

// Wait until |matcher| is accessible (not nil).
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

// Creates a new main tab and load |url|. Wait until |word| is visible on the
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
}  // namespace

// Test for the TabUsageRecorder class.
@interface TabUsageRecorderTestCase : ChromeTestCase
@end

@implementation TabUsageRecorderTestCase

// Tests that the recorder actual recorde tab state.
// TODO(crbug.com/934228) The test is flaky.
- (void)DISABLED_testTabSwitchRecorder {
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
  [ChromeEarlGrey resetTabUsageRecorder];
  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  // Open two tabs with urls.
  OpenTwoTabs();
  [ChromeEarlGrey waitForMainTabCount:2];
  // Switch between the two tabs.  Both are currently in memory.
  [ChromeEarlGrey selectTabAtIndex:0];

  // Verify that one in-memory tab switch has been recorded.
  // histogramTester.ExpectTotalCount(kSelectedTabHistogramName, 1,
  // failureBlock);
  histogramTester.ExpectUniqueSample(
      kSelectedTabHistogramName, TabUsageRecorder::IN_MEMORY, 1, failureBlock);

  // Evict the tab.
  GREYAssertTrue(OpenNewIncognitoTabUsingUIAndEvictMainTabs(),
                 @"Failed to open Incognito Tab");

  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Failed to switch to incognito mode");

  // Switch back to the normal tabs. Should be on tab one.
  SwitchToNormalMode();

  [ChromeEarlGrey waitForWebStateContainingText:kURL1FirstWord];

  histogramTester.ExpectTotalCount(kSelectedTabHistogramName, 2, failureBlock);
  histogramTester.ExpectBucketCount(kSelectedTabHistogramName,
                                    TabUsageRecorder::EVICTED, 1, failureBlock);
}

// Verifies the UMA metric for page loads before a tab eviction by loading
// some tabs, forcing a tab eviction, then checking the histogram.
- (void)testPageLoadCountBeforeEvictedTab {
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
  [ChromeEarlGrey resetTabUsageRecorder];
  const GURL url1 = web::test::HttpServer::MakeUrl(kTestUrl1);
  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

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
  histogramTester.ExpectTotalCount(kPageLoadsBeforeEvictedTabSelected, 0,
                                   failureBlock);

  // Reload each tab.
  for (NSUInteger i = 0; i < numberOfTabs; i++) {
    [ChromeEarlGrey selectTabAtIndex:i];
    // Clear the page so that we can check when page reload is complete.
    __block bool finished = false;
    chrome_test_util::GetCurrentWebState()->ExecuteJavaScript(
        base::UTF8ToUTF16(kClearPageScript),
        base::BindOnce(^(const base::Value*) {
          finished = true;
        }));

    GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(1.0,
                                                            ^{
                                                              return finished;
                                                            }),
               @"JavaScript to reload each tab did not finish");
    [ChromeEarlGreyUI reload];
    [ChromeEarlGrey waitForWebStateContainingText:kURL1FirstWord];
  }

  // Evict the tab. Create a dummy tab so that switching back to normal mode
  // does not trigger a reload immediately.
  [ChromeEarlGrey openNewTab];
  GREYAssertTrue(OpenNewIncognitoTabUsingUIAndEvictMainTabs(),
                 @"Failed to open Incognito Tab");
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  // Switch back to the normal tabs. Should be on tab one.
  SwitchToNormalMode();

  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:kURL1FirstWord];

  // Verify that one page-load count has been recorded. It should contain two
  // page loads for each tab created.
  histogramTester.ExpectTotalCount(kPageLoadsBeforeEvictedTabSelected, 1,
                                   failureBlock);

  std::unique_ptr<base::HistogramSamples> samples =
      histogramTester.GetHistogramSamplesSinceCreation(
          kPageLoadsBeforeEvictedTabSelected);
  int sampleSum = samples ? samples->sum() : 0;
  GREYAssertEqual(sampleSum, numberOfTabs * 2,
                  @"Expected page loads is %d, actual %d.", numberOfTabs * 2,
                  sampleSum);
}

// Tests that tabs reloaded on cold start are reported as
// EVICTED_DUE_TO_COLD_START.
// TODO(crbug.com/934228) The test is disabled due to flakiness.
- (void)DISABLED_testColdLaunchReloadCount {
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
  [ChromeEarlGrey resetTabUsageRecorder];

  // Open two tabs with urls.
  OpenTwoTabs();
  [ChromeEarlGrey waitForMainTabCount:2];
  // Set the normal tabs as 'cold start' tabs.
  [ChromeEarlGrey setCurrentTabsToBeColdStartTabs];

  // Open two incognito tabs with urls, clearing normal tabs from memory.
  GREYAssertTrue(OpenNewIncognitoTabUsingUIAndEvictMainTabs(),
                 @"Failed to open first Incognito Tab");
  GREYAssertTrue(OpenNewIncognitoTabUsingUIAndEvictMainTabs(),
                 @"Failed to open second Incognito Tab");

  [ChromeEarlGrey waitForIncognitoTabCount:2];

  // Switch back to the normal tabs.
  SwitchToNormalMode();

  [ChromeEarlGrey waitForWebStateContainingText:kURL2FirstWord];

  // Select the other one so it also reloads.
  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:kURL1FirstWord];
  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };
  // Make sure that one of the 2 tab loads (excluding the selected tab) is
  // counted as a cold start eviction.
  histogramTester.ExpectBucketCount(kSelectedTabHistogramName,
                                    TabUsageRecorder::EVICTED_DUE_TO_COLD_START,
                                    1, failureBlock);

  histogramTester.ExpectBucketCount(
      kSelectedTabHistogramName, TabUsageRecorder::IN_MEMORY, 0, failureBlock);
  // Re-select the same tab and make sure it is not counted again as evicted.
  [ChromeEarlGrey selectTabAtIndex:1];
  [ChromeEarlGrey selectTabAtIndex:0];

  [ChromeEarlGrey waitForWebStateContainingText:kURL1FirstWord];
  histogramTester.ExpectBucketCount(kSelectedTabHistogramName,
                                    TabUsageRecorder::EVICTED_DUE_TO_COLD_START,
                                    1, failureBlock);

  histogramTester.ExpectBucketCount(
      kSelectedTabHistogramName, TabUsageRecorder::IN_MEMORY, 2, failureBlock);
}

// Tests that tabs reloads after backgrounding and eviction.
// TODO(crbug.com/934228) The test is flaky.
- (void)DISABLED_testBackgroundingReloadCount {
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
  [ChromeEarlGrey resetTabUsageRecorder];
  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  // Open two tabs with urls.
  OpenTwoTabs();
  [ChromeEarlGrey waitForMainTabCount:2];

  // Simulate going into the background.
  [ChromeEarlGrey simulateTabsBackgrounding];

  // Open incognito and clear normal tabs from memory.
  GREYAssertTrue(OpenNewIncognitoTabUsingUIAndEvictMainTabs(),
                 @"Failed to open Incognito Tab");
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Failed to switch to incognito mode");
  histogramTester.ExpectTotalCount(kEvictedTabReloadTime, 0, failureBlock);

  // Switch back to the normal tabs.
  SwitchToNormalMode();

  [ChromeEarlGrey waitForWebStateContainingText:kURL2FirstWord];

  const GURL url1 = web::test::HttpServer::MakeUrl(kTestUrl1);
  const GURL url2 = web::test::HttpServer::MakeUrl(kTestUrl2);
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(url2.GetContent())]
      assertWithMatcher:grey_notNil()];
  histogramTester.ExpectTotalCount(kEvictedTabReloadTime, 1, failureBlock);

  [ChromeEarlGrey selectTabAtIndex:0];
  [ChromeEarlGrey waitForWebStateContainingText:kURL1FirstWord];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(url1.GetContent())]
      assertWithMatcher:grey_notNil()];
  histogramTester.ExpectTotalCount(kEvictedTabReloadTime, 2, failureBlock);
}

// Verify correct recording of metrics when the reloading of an evicted tab
// succeeds.
// TODO(crbug.com/934228) The test is flaky.
- (void)DISABLED_testEvictedTabReloadSuccess {
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  [ChromeEarlGrey closeAllTabsInCurrentMode];
  GURL URL = web::test::HttpServer::MakeUrl(kTestUrl1);
  NewMainTabWithURL(URL, kURL1FirstWord);
  GREYAssertTrue(OpenNewIncognitoTabUsingUIAndEvictMainTabs(),
                 @"Failed to open Incognito Tab");
  SwitchToNormalMode();

  [ChromeEarlGrey waitForWebStateContainingText:kURL1FirstWord];
  [ChromeEarlGrey waitForMainTabCount:1];

  histogramTester.ExpectUniqueSample(kEvictedTabReloadSuccessRate,
                                     TabUsageRecorder::LOAD_SUCCESS, 1,
                                     failureBlock);
  histogramTester.ExpectUniqueSample(kDidUserWaitForEvictedTabReload,
                                     TabUsageRecorder::USER_WAITED, 1,
                                     failureBlock);
  histogramTester.ExpectTotalCount(kEvictedTabReloadTime, 1, failureBlock);
}

// Test that USER_DID_NOT_WAIT is reported if the user does not wait for the
// reload to be complete after eviction.
- (void)testEvictedTabSlowReload {
  std::map<GURL, std::string> responses;
  const GURL slowURL = web::test::HttpServer::MakeUrl("http://slow");
  responses[slowURL] = "Slow Page";

  web::test::SetUpHttpServer(std::make_unique<web::DelayedResponseProvider>(
      std::make_unique<HtmlResponseProvider>(responses), kSlowURLDelay));

  chrome_test_util::HistogramTester histogramTester;
  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  // A blank tab needed to switch to it after reloading.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:slowURL waitForCompletion:NO];
  GREYAssertTrue(OpenNewIncognitoTabUsingUIAndEvictMainTabs(),
                 @"Failed to open Incognito Tab");

  web::test::SetUpHttpServer(std::make_unique<web::DelayedResponseProvider>(
      std::make_unique<HtmlResponseProvider>(responses), kSlowURLDelay));

  SwitchToNormalMode();
  {
    // Turn off synchronization of GREYAssert to test the pending states.
    ScopedSynchronizationDisabler disabler;
    GREYAssert(
        [[GREYCondition conditionWithName:@"Wait for tab to restart loading."
                                    block:^BOOL() {
                                      return [ChromeEarlGrey isLoading];
                                    }] waitWithTimeout:kWaitElementTimeout],
        @"Tab did not start loading.");
  }

  // This method is not synced on EarlGrey.
  [ChromeEarlGrey selectTabAtIndex:0];

  // Do not test the kEvictedTabReloadSuccessRate, as the timing of the two
  // page loads cannot be guaranteed.  The test would be flaky.
  histogramTester.ExpectBucketCount(kDidUserWaitForEvictedTabReload,
                                    TabUsageRecorder::USER_DID_NOT_WAIT, 1,
                                    failureBlock);
}

// Test that the USER_DID_NOT_WAIT metric is logged when the user opens an NTP
// while the evicted tab is still reloading.
- (void)testEvictedTabReloadSwitchToNTP {
  std::map<GURL, std::string> responses;
  const GURL slowURL = web::test::HttpServer::MakeUrl("http://slow");
  responses[slowURL] = "Slow Page";

  web::test::SetUpHttpServer(std::make_unique<HtmlResponseProvider>(responses));

  chrome_test_util::HistogramTester histogramTester;
  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  NewMainTabWithURL(slowURL, "Slow");

  GREYAssertTrue(OpenNewIncognitoTabUsingUIAndEvictMainTabs(),
                 @"Failed to open Incognito Tab");

  web::test::SetUpHttpServer(std::make_unique<web::DelayedResponseProvider>(
      std::make_unique<HtmlResponseProvider>(responses), kVerySlowURLDelay));

  [ChromeEarlGrey removeBrowsingCache];

  SwitchToNormalMode();

  // Letting page load start.
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(0.5));

  // TODO(crbug.com/640977): EarlGrey synchronize on some animations when a
  // page is loading. Need to handle synchronization manually for this test.
  {
    ScopedSynchronizationDisabler disabler;
    // Make sure the button is here and displayed before tapping it.
    id<GREYMatcher> toolMenuMatcher =
        grey_allOf(grey_accessibilityID(kToolbarToolsMenuButtonIdentifier),
                   grey_sufficientlyVisible(), nil);
    Wait(toolMenuMatcher, @"Tool Menu");

    [ChromeEarlGrey openNewTab];
  }
  histogramTester.ExpectBucketCount(kDidUserWaitForEvictedTabReload,
                                    TabUsageRecorder::USER_DID_NOT_WAIT, 1,
                                    failureBlock);
}

// Test that the USER_DID_NOT_WAIT metric is not logged when the user opens
// and closes the settings UI while the evicted tab is still reloading.
- (void)testEvictedTabReloadSettingsAndBack {
  std::map<GURL, std::string> responses;
  const GURL slowURL = web::test::HttpServer::MakeUrl("http://slow");
  responses[slowURL] = "Slow Page";

  web::test::SetUpHttpServer(std::make_unique<web::DelayedResponseProvider>(
      std::make_unique<HtmlResponseProvider>(responses), kSlowURLDelay));

  chrome_test_util::HistogramTester histogramTester;
  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };

  NewMainTabWithURL(slowURL, responses[slowURL]);
  GREYAssertTrue(OpenNewIncognitoTabUsingUIAndEvictMainTabs(),
                 @"Failed to open Incognito Tab");

  SwitchToNormalMode();

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:responses[slowURL]];

  histogramTester.ExpectBucketCount(kDidUserWaitForEvictedTabReload,
                                    TabUsageRecorder::USER_DID_NOT_WAIT, 0,
                                    failureBlock);
  histogramTester.ExpectBucketCount(kDidUserWaitForEvictedTabReload,
                                    TabUsageRecorder::USER_WAITED, 1,
                                    failureBlock);
}

// Tests that leaving Chrome while an evicted tab is reloading triggers the
// recording of the USER_LEFT_CHROME metric.
- (void)testEvictedTabReloadBackgrounded {
  std::map<GURL, std::string> responses;
  const GURL slowURL = web::test::HttpServer::MakeUrl("http://slow");
  responses[slowURL] = "Slow Page";

  web::test::SetUpHttpServer(std::make_unique<HtmlResponseProvider>(responses));

  chrome_test_util::HistogramTester histogramTester;
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:slowURL waitForCompletion:NO];
  GREYAssertTrue(OpenNewIncognitoTabUsingUIAndEvictMainTabs(),
                 @"Failed to open Incognito Tab");

  web::test::SetUpHttpServer(std::make_unique<web::DelayedResponseProvider>(
      std::make_unique<HtmlResponseProvider>(responses), kVerySlowURLDelay));

  [ChromeEarlGrey removeBrowsingCache];

  SwitchToNormalMode();

  // Letting page load start.
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(0.5));

  // TODO(crbug.com/640977): EarlGrey synchronize on some animations when a
  // page is loading. Need to handle synchronization manually for this test.
  {
    ScopedSynchronizationDisabler disabler;
    id<GREYMatcher> toolMenuMatcher =
        grey_allOf(grey_accessibilityID(kToolbarToolsMenuButtonIdentifier),
                   grey_sufficientlyVisible(), nil);
    Wait(toolMenuMatcher, @"Tool Menu");

    [ChromeEarlGrey simulateTabsBackgrounding];
  }

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };
  histogramTester.ExpectBucketCount(kDidUserWaitForEvictedTabReload,
                                    TabUsageRecorder::USER_LEFT_CHROME, 1,
                                    failureBlock);
}

// Tests that backgrounding a tab that was not evicted while it is loading does
// not record the USER_LEFT_CHROME metric.
- (void)testLiveTabReloadBackgrounded {
  std::map<GURL, std::string> responses;
  const GURL slowURL = web::test::HttpServer::MakeUrl("http://slow");
  responses[slowURL] = "Slow Page";

  web::test::SetUpHttpServer(std::make_unique<web::DelayedResponseProvider>(
      std::make_unique<HtmlResponseProvider>(responses), kSlowURLDelay));

  chrome_test_util::HistogramTester histogramTester;

  // We need two tabs to be able to switch.
  [ChromeEarlGrey openNewTab];
  {
    ScopedSynchronizationDisabler disabler;
    [ChromeEarlGrey loadURL:slowURL waitForCompletion:NO];

    // Ensure loading starts but is not finished.
    base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSeconds(1));
    [ChromeEarlGrey selectTabAtIndex:0];
  }

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };
  histogramTester.ExpectBucketCount(kDidUserWaitForEvictedTabReload,
                                    TabUsageRecorder::USER_LEFT_CHROME, 0,
                                    failureBlock);
}

// Tests that redirecting pages are not reloaded after eviction.
- (void)testPageRedirect {
  GURL redirectURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/redirect_refresh.html");
  GURL destinationURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
  [ChromeEarlGrey resetTabUsageRecorder];

  NewMainTabWithURL(redirectURL, "arrived");

  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  NSUInteger tabIndex = [ChromeEarlGrey mainTabCount] - 1;
  [ChromeEarlGrey openNewTab];
  GREYAssertTrue(OpenNewIncognitoTabUsingUIAndEvictMainTabs(),
                 @"Failed to open Incognito Tab");

  SwitchToNormalMode();

  [ChromeEarlGrey selectTabAtIndex:tabIndex];
  [ChromeEarlGrey waitForWebStateContainingText:"arrived"];

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };
  // Verify that one page-load count has been recorded.  It should contain a
  // sum of 1 - one sample with 1 page load.
  histogramTester.ExpectTotalCount(kPageLoadsBeforeEvictedTabSelected, 1,
                                   failureBlock);

  std::unique_ptr<base::HistogramSamples> samples =
      histogramTester.GetHistogramSamplesSinceCreation(
          kPageLoadsBeforeEvictedTabSelected);
  int sampleSum = samples->sum();
  GREYAssertEqual(sampleSum, 1, @"Expected page loads is %d, actual is %d.", 1,
                  sampleSum);
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
  chrome_test_util::HistogramTester histogramTester;
  [ChromeEarlGrey resetTabUsageRecorder];

  // Open a tab with a link to click.
  NewMainTabWithURL(initialURL, "link");
  // Click the link.
  [ChromeEarlGrey tapWebStateElementWithID:@"link"];

  [ChromeEarlGrey waitForWebStateContainingText:"Whee"];
  NSUInteger tabIndex = [ChromeEarlGrey mainTabCount] - 1;
  [ChromeEarlGrey openNewTab];
  GREYAssertTrue(OpenNewIncognitoTabUsingUIAndEvictMainTabs(),
                 @"Failed to open Incognito Tab");
  SwitchToNormalMode();

  [ChromeEarlGrey selectTabAtIndex:tabIndex];
  [ChromeEarlGrey waitForWebStateContainingText:"Whee"];

  // Verify that the page-load count has been recorded.  It should contain a
  // sum of 2 - one sample with 2 page loads.
  std::unique_ptr<base::HistogramSamples> samples =
      histogramTester.GetHistogramSamplesSinceCreation(
          kPageLoadsBeforeEvictedTabSelected);
  int sampleSum = samples->sum();
  GREYAssertEqual(sampleSum, 2, @"Expected page loads is %d, actual %d.", 2,
                  sampleSum);

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };
  // Verify that only one evicted tab was selected.  This is to make sure the
  // link click did not generate an evicted-tab-reload count.
  histogramTester.ExpectBucketCount(kSelectedTabHistogramName,
                                    TabUsageRecorder::EVICTED, 1, failureBlock);
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
  chrome_test_util::HistogramTester histogramTester;
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

  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  [ChromeEarlGrey waitForWebStateContainingText:"Whee"];

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };
  histogramTester.ExpectTotalCount(kSelectedTabHistogramName, 1, failureBlock);
  histogramTester.ExpectBucketCount(
      kSelectedTabHistogramName, TabUsageRecorder::IN_MEMORY, 1, failureBlock);
}

// Tests that opening tabs from external app will not cause tab eviction.
- (void)testOpenFromApp {
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
  [ChromeEarlGrey resetTabUsageRecorder];

  [ChromeEarlGrey openNewTab];
  GURL url(kTestUrl1);

  chrome_test_util::OpenChromeFromExternalApp(url);

  // Add a delay to ensure the tab has fully opened.  Because the check below
  // is for zero metrics recorded, it adds no flakiness.  However, this pause
  // makes the step more likely to fail in failure cases.  I.e. without it, this
  // test would sometimes pass even when it should fail.
  base::test::ios::SpinRunLoopWithMaxDelay(
      base::TimeDelta::FromMilliseconds(500));

  FailureBlock failureBlock = ^(NSString* error) {
    GREYFail(error);
  };
  // Verify that zero Tab.StatusWhenSwitchedBackToForeground metrics were
  // recorded.  Tabs created at the time the user switches to them should not
  // be counted in this metric.
  histogramTester.ExpectTotalCount(kSelectedTabHistogramName, 0, failureBlock);
}

// Verify that evicted tabs that are deleted are removed from the evicted tabs
// map.
- (void)testTabDeletion {
  web::test::SetUpFileBasedHttpServer();
  chrome_test_util::HistogramTester histogramTester;
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
    // tracked
    // by the tab usage recorder in its |evicted_tabs_| map.
    CloseTabAtIndexAndSync(1);

    GREYAssertEqual([ChromeEarlGrey mainTabCount], 2,
                    @"Check number of normal tabs");
    CloseTabAtIndexAndSync(0);
    GREYAssertEqual([ChromeEarlGrey mainTabCount], 1,
                    @"Check number of normal tabs");
    [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  }
  // The deleted tabs are purged during foregrounding and backgrounding.
  [ChromeEarlGrey simulateTabsBackgrounding];
  // Make sure |evicted_tabs_| purged the deleted tabs.
  int evicted = [ChromeEarlGrey evictedMainTabCount];
  GREYAssertEqual(evicted, 0, @"Check number of evicted tabs");
}

@end
