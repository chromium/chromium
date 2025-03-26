// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "components/metrics/dwa/dwa_recorder.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/default_handlers.h"

using chrome_test_util::GoogleServicesSettingsButton;
using chrome_test_util::SettingsDoneButton;

@interface DWATestCase : ChromeTestCase

@end

@implementation DWATestCase

+ (void)setUpForTestCase {
  [super setUpForTestCase];
}

- (void)setUp {
  [super setUp];

  // These tests enable history synchronization via Recent Tabs.
  // If the list contains too many tabs, the button at the bottom of
  // the view moves offscreen, and its animation causes the tests to freeze,
  // similar to crbug.com/640977.
  if (![ChromeTestCase forceRestartAndWipe]) {
    [ChromeEarlGrey clearBrowsingHistory];
  }

  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:NO],
             @"setUp: Failed to assert that DWA was not enabled.");

  // Sign in to Chrome and enable history sync.

  // Note: URL-keyed anonymized data collection is turned on as part of the
  // flow to Sign in to Chrome and enable history sync. This matches the main
  // user flow that enables DWA.
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                         enableHistorySync:YES];
  [ChromeEarlGrey waitForSyncTransportStateActiveWithTimeout:
                      syncher::kSyncDWAOperationsTimeout];

  // Grant metrics consent and update MetricsServicesManager.
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];

  GREYAssert(![MetricsAppInterface setMetricsAndCrashReportingForTesting:YES],
             @"setUp: Unpaired set/reset of user consent.");

  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:YES],
             @"setUp: Failed to assert that DWA was enabled.");
  GREYAssert([MetricsAppInterface DWARecorderAllowedForAllProfiles:YES],
             @"setUp: Failed to assert that DWA was allowed for all profiles.");

  net::test_server::RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

- (void)tearDownHelper {
  [ChromeEarlGrey waitForSyncTransportStateActiveWithTimeout:
                      syncher::kSyncDWAOperationsTimeout];
  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:YES],
             @"tearDownHelper: Failed to assert that DWA was enabled.");
  [MetricsAppInterface clearDWARecorder];
  [self assertDwaRecorderIsEmpty];

  // Revoke metrics consent and update MetricsServicesManager.
  GREYAssert([MetricsAppInterface setMetricsAndCrashReportingForTesting:NO],
             @"tearDownHelper: Unpaired set/reset of user consent.");
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:NO],
             @"tearDownHelper: Failed to assert that DWA was not enabled.");

  // Sign out of Chrome.

  // Note: URL-keyed anonymized data collection is turned off as part of the
  // flow to Sign out of Chrome. This matches the main user flow that disables
  // DWA.
  [SigninEarlGrey signOut];

  [ChromeEarlGrey clearFakeSyncServerData];

  [super tearDownHelper];
}

// Enable the DWA feature
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  config.features_enabled.push_back(metrics::dwa::kDwaFeature);

  return config;
}

#pragma mark - Helpers

// Waits for a new incognito tab to be opened.
- (void)openNewIncognitoTab {
  const NSUInteger incognitoTabCount = [ChromeEarlGrey incognitoTabCount];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:(incognitoTabCount + 1)];
  GREYAssert([ChromeEarlGrey isIncognitoMode],
             @"openNewIncognitoTab: Failed to switch to incognito mode.");
}

// Waits for the current incognito tab to be closed.
- (void)closeCurrentIncognitoTab {
  const NSUInteger incognitoTabCount = [ChromeEarlGrey incognitoTabCount];
  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey waitForIncognitoTabCount:(incognitoTabCount - 1)];
}

// Waits for all incognito tabs to be closed.
- (void)closeAllIncognitoTabs {
  [ChromeEarlGrey closeAllIncognitoTabs];
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  // The user is dropped into the tab grid after closing the last incognito tab.
  // Therefore this test must manually switch back to showing the normal tabs.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
  GREYAssert(![ChromeEarlGrey isIncognitoMode],
             @"closeAllIncognitoTabs: Failed to switch to normal mode.");
}

// Waits for a new tab to be opened.
- (void)openNewRegularTab {
  const NSUInteger tabCount = [ChromeEarlGrey mainTabCount];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:(tabCount + 1)];
}

// Records a test DWA entry metric and a pageload event.
- (void)recordTestDWAEntryMetricAndPageLoadEvent {
  [MetricsAppInterface recordTestDWAEntryMetric];
  [MetricsAppInterface DWARecorderOnPageLoadCall];
  [MetricsAppInterface recordTestDWAEntryMetric];
}

// Toggle "Make searches and browsing better" switch on.
- (void)turnOnMsbbSwitch {
  // Wait for the Msbb switch to appear.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::TableViewSwitchCell(
                          @"betterSearchAndBrowsingItem_switch", NO)];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   @"betterSearchAndBrowsingItem_switch", NO)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];
}

// Toggle "Make searches and browsing better" switch off.
- (void)turnOffMsbbSwitch {
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::TableViewSwitchCell(
                          @"betterSearchAndBrowsingItem_switch", YES)];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   @"betterSearchAndBrowsingItem_switch", YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
}

// Assertions to check that the DWA recorder is enabled and allowed for all
// profiles.
- (void)assertDwaIsEnabledAndAllowed {
  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:YES],
             @"Failed to assert that DWA was enabled.");
  GREYAssert([MetricsAppInterface DWARecorderAllowedForAllProfiles:YES],
             @"Failed to assert that DWA was "
             @"allowed for all profiles.");
}

// Assertions to check that the DWA recorder has page load events and entries.
- (void)assertDwaRecorderHasMetrics {
  GREYAssert([MetricsAppInterface DWARecorderHasPageLoadEvents:YES],
             @"DWA Recorder should have "
             @"pageload events.");
  GREYAssert([MetricsAppInterface DWARecorderHasEntries:YES],
             @"DWA Recorder should have "
             @"entries.");
}

// Assertions to check that the DWA recorder is not enabled and not allowed for
// all profiles.
- (void)assertDwaIsDisabledAndDisallowed {
  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:NO],
             @"Failed to assert that DWA was not enabled.");
  GREYAssert([MetricsAppInterface DWARecorderAllowedForAllProfiles:NO],
             @"Failed to assert that DWA was not "
             @"allowed for all profiles.");
}

// Assertions to check that the DWA recorder has no page load events or entries.
- (void)assertDwaRecorderIsEmpty {
  GREYAssert([MetricsAppInterface DWARecorderHasPageLoadEvents:NO],
             @"DWA Recorder should not have any "
             @"pageload events.");
  GREYAssert([MetricsAppInterface DWARecorderHasEntries:NO],
             @"DWA Recorder should not have any "
             @"entries.");
}

// Assertions to check that the DWA recorder has entries but no page load
// events.
- (void)assertDwaRecorderHasEntriesAndNoPageloadEvents {
  GREYAssert([MetricsAppInterface DWARecorderHasEntries:YES],
             @"DWA Recorder should have entries.");
  GREYAssert([MetricsAppInterface DWARecorderHasPageLoadEvents:NO],
             @"DWA Recorder should not have pageload events.");
}

// Assertions to check that the DWA recorder has page load events but no
// entries.
- (void)assertDwaRecorderHasPageLoadEventsAndNoEntries {
  GREYAssert([MetricsAppInterface DWARecorderHasEntries:NO],
             @"DWA Recorder should not have any entries.");
  GREYAssert([MetricsAppInterface DWARecorderHasPageLoadEvents:YES],
             @"DWA Recorder should have pageload events.");
}

#pragma mark - Tests

// The tests in this file should correspond to the tests in
// //chrome/browser/metrics/dwa_browsertest.cc

// LINT.IfChange(DwaServiceCheck)
- (void)testDwaServiceCheck {
  // [Note: Tests begin with an open regular tab. This tab is opened in setUp.
  // DWA should be enabled with first regular browser.
  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:YES],
             @"Failed to assert that DWA was enabled.");

  // Records a DWA entry metric.
  [MetricsAppInterface recordTestDWAEntryMetric];
  [self assertDwaRecorderHasEntriesAndNoPageloadEvents];
  GREYAssert([MetricsAppInterface hasUnsentDWALogs:NO],
             @"DWA Service should not have unsent logs.");

  // opening a new regular tab automatically navigates to a new URL to simulate
  // page load action.
  [self openNewRegularTab];
  [self assertDwaRecorderHasPageLoadEventsAndNoEntries];
  GREYAssert([MetricsAppInterface hasUnsentDWALogs:NO],
             @"DWA Service should not have unsent logs.");

  [MetricsAppInterface DWAServiceFlushCall];
  [self assertDwaRecorderIsEmpty];
  GREYAssert([MetricsAppInterface hasUnsentDWALogs:YES],
             @"DWA Service should have unsent logs.");
}
// LINT.ThenChange(/chrome/browser/metrics/dwa_browsertest.cc:DwaServiceCheck)

// Make sure that DWA is disabled and purged while an incognito window is open.
// LINT.IfChange(RegularBrowserPlusIncognitoCheck)
- (void)testRegularBrowserPlusIncognitoCheck {
  // Note: Tests begin with an open regular tab. This tab is opened in setUp.
  // DWA should be enabled with first regular browser.
  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:YES],
             @"Failed to assert that DWA "
             @"was enabled.");
  [MetricsAppInterface recordTestDWAEntryMetric];
  GREYAssert([MetricsAppInterface DWARecorderHasEntries:YES],
             @"Failed to record test "
             @"entry metric.");

  // Opening an incognito browser should disable DwaRecorder and metrics
  // should be purged.
  [self openNewIncognitoTab];
  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:NO],
             @"Failed to assert that DWA "
             @"was not enabled.");
  GREYAssert([MetricsAppInterface DWARecorderHasEntries:NO],
             @"DWA Recorder should not "
             @"have any entries.");
  [MetricsAppInterface recordTestDWAEntryMetric];
  GREYAssert([MetricsAppInterface DWARecorderHasEntries:NO],
             @"DWA Recorder should not "
             @"have any entries.");

  // Opening another regular tab mustn't enabled DWA.
  [self openNewRegularTab];
  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:NO],
             @"Failed to assert that DWA was not enabled.");
  [MetricsAppInterface recordTestDWAEntryMetric];
  GREYAssert([MetricsAppInterface DWARecorderHasEntries:NO],
             @"DWA Recorder should not "
             @"have any entries.");

  // Opening and closing another incognito browser must not enable DWA.
  [self openNewIncognitoTab];
  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:NO],
             @"Failed to assert that DWA was not enabled.");
  [MetricsAppInterface recordTestDWAEntryMetric];
  GREYAssert([MetricsAppInterface DWARecorderHasEntries:NO],
             @"DWA Recorder should not "
             @"have any entries.");

  [self closeCurrentIncognitoTab];
  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:NO],
             @"Failed to assert that DWA was not enabled.");
  [MetricsAppInterface recordTestDWAEntryMetric];
  GREYAssert([MetricsAppInterface DWARecorderHasEntries:NO],
             @"DWA Recorder should not "
             @"have any entries.");

  // TODO(crbug.com/41271925): Due to continuous animations, it is not feasible
  // (i) to use the tab switcher to switch between modes or (ii) to omit the
  // below code block and simply call [ChromeEarlGrey closeAllIncognitoTabs];
  // from incognito mode.
  [self openNewRegularTab];
  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey closeCurrentTab];
  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:NO],
             @"Failed to assert that DWA "
             @"was not enabled.");
  [MetricsAppInterface recordTestDWAEntryMetric];
  GREYAssert([MetricsAppInterface DWARecorderHasEntries:NO],
             @"DWA Recorder should not "
             @"have any entries.");

  // At this point, there is one open regular tab and one open incognito tab.
  [ChromeEarlGrey closeAllIncognitoTabs];
  // All incognito tabs have been closed, so DWA should be enabled.
  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:YES],
             @"Failed to assert that DWA "
             @"was enabled.");
  GREYAssert([MetricsAppInterface DWARecorderHasEntries:NO],
             @"DWA Recorder should not "
             @"have any entries.");
  [MetricsAppInterface recordTestDWAEntryMetric];
  GREYAssert([MetricsAppInterface DWARecorderHasEntries:YES],
             @"Failed to record test "
             @"entry metric.");
}
// LINT.ThenChange(/chrome/browser/metrics/dwa_browsertest.cc:RegularBrowserPlusIncognitoCheck)

// Make sure opening a regular browser after Incognito doesn't enable DWA.
// LINT.IfChange(IncognitoBrowserPlusRegularCheck)
- (void)testIncognitoBrowserPlusRegularCheck {
  // Note: Tests begin with an open regular tab. This tab is opened in setUp.

  // TODO(crbug.com/41271925): Due to continuous animations, it is not feasible
  // to close the regular tab that is already open. The functions closeAllTabs,
  // closeCurrentTab, and closeAllTabsInCurrentMode close the tab and then hang.
  // As a workaround, we open an incognito tab and then close the regular tab to
  // get to a state in which a single incognito tab is open.

  // Opening an incognito browser should disable DwaRecorder and metrics should
  // be purged.
  [self openNewIncognitoTab];
  [ChromeEarlGrey closeAllNormalTabs];
  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:NO],
             @"Failed to assert that DWA was not enabled.");

  // Opening another regular browser must not enable DWA.
  [self openNewRegularTab];
  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:NO],
             @"Failed to assert that DWA was not enabled.");

  // All incognito tabs have been closed, so DWA should be enabled.
  [ChromeEarlGrey closeAllIncognitoTabs];
  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:YES],
             @"Failed to assert that DWA was enabled.");
}
// LINT.ThenChange(/chrome/browser/metrics/dwa_browsertest.cc:IncognitoBrowserPlusRegularCheck)

// Tests that disabling MSBB UKM consent disables and purges DWA.
// Additionally tests that DWA is disabled until all UKM consents are enabled.
// LINT.IfChange(UkmMsbbConsentChangeCheck)
- (void)testUkmMsbbConsentChangeCheck {
  // Note: Tests begin with an open regular tab. This tab is opened in setUp.
  [self recordTestDWAEntryMetricAndPageLoadEvent];
  [self assertDwaIsEnabledAndAllowed];
  [self assertDwaRecorderHasMetrics];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      GoogleServicesSettingsButton()];

  // Open Google services settings.
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Toggle "Make searches and browsing better" switch off.
  [self turnOffMsbbSwitch];
  [self assertDwaIsDisabledAndDisallowed];
  [self assertDwaRecorderIsEmpty];

  // Toggle "Make searches and browsing better" switch on.
  [self turnOnMsbbSwitch];
  [self assertDwaIsEnabledAndAllowed];
  [self assertDwaRecorderIsEmpty];

  // Validate DWA entries and page load events are able to be recorded when all
  // consents are enabled.
  [self recordTestDWAEntryMetricAndPageLoadEvent];
  [self assertDwaRecorderHasMetrics];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}
// LINT.ThenChange(/chrome/browser/metrics/dwa_browsertest.cc:UkmMsbbConsentChangeCheck)

// Tests that reloading the page triggers a call to DWA pageload.
- (void)testDwaRecorderPageLoadTriggeredWhenReloadingPage {
  GREYAssert([MetricsAppInterface checkDWARecordingEnabled:YES],
             @"Failed to assert that DWA "
             @"was enabled.");
  [MetricsAppInterface recordTestDWAEntryMetric];
  [self assertDwaRecorderHasEntriesAndNoPageloadEvents];

  [self openNewRegularTab];
  [self assertDwaRecorderHasPageLoadEventsAndNoEntries];
  [MetricsAppInterface clearDWARecorder];

  [MetricsAppInterface recordTestDWAEntryMetric];
  [self assertDwaRecorderHasEntriesAndNoPageloadEvents];

  // Loads simple page on localhost
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/test_url.html")];
  [self assertDwaRecorderHasPageLoadEventsAndNoEntries];
  [MetricsAppInterface clearDWARecorder];

  [MetricsAppInterface recordTestDWAEntryMetric];
  [ChromeEarlGrey reload];
  [self assertDwaRecorderHasPageLoadEventsAndNoEntries];
}

@end
