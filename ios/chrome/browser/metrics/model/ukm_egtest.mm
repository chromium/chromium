// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using chrome_test_util::GoogleServicesSettingsButton;
using chrome_test_util::SettingsDoneButton;

@interface UKMTestCase : ChromeTestCase

@end

@implementation UKMTestCase

+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [self setUpHelper];
}

+ (void)setUpHelper {
  if (![ChromeEarlGrey isUKMEnabled]) {
    // ukm::kUkmFeature feature is not enabled. You need to pass
    // --enable-features=Ukm command line argument in order to run this test.
    DCHECK(false);
  }
}

- (void)setUp {
  [super setUp];

  // These tests enable history sync through Recent Tabs. If there are too many
  // tabs in the list, the button at the bottom of the view is offscreen and its
  // animation causes tests to hang for the same reasons as crbug.com/640977.
  // Clear browsing history to ensure that there are no recent tabs.
  [ChromeEarlGrey clearBrowsingHistory];

  [ChromeEarlGrey
      waitForSyncEngineInitialized:NO
                       syncTimeout:syncher::kSyncUKMOperationsTimeout];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");
  // Sign in to Chrome and enable history sync.
  //
  // Note: URL-keyed anonymized data collection is turned on as part of the
  // flow to Sign in to Chrome and enable history sync. This matches the main
  // user flow that enables UKM.
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                         enableHistorySync:YES];
  [ChromeEarlGrey
      waitForSyncEngineInitialized:YES
                       syncTimeout:syncher::kSyncUKMOperationsTimeout];

  // Grant metrics consent and update MetricsServicesManager.
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];

  GREYAssert(![MetricsAppInterface setMetricsAndCrashReportingForTesting:YES],
             @"Unpaired set/reset of user consent.");
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");
}

- (void)tearDown {
  [ChromeEarlGrey
      waitForSyncEngineInitialized:YES
                       syncTimeout:syncher::kSyncUKMOperationsTimeout];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");

  // Revoke metrics consent and update MetricsServicesManager.
  GREYAssert([MetricsAppInterface setMetricsAndCrashReportingForTesting:NO],
             @"Unpaired set/reset of user consent.");
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  // Sign out of Chrome.
  //
  // Note: URL-keyed anonymized data collection is turned off as part of the
  // flow to Sign out of Chrome. This matches the main user flow that disables
  // UKM.
  [SigninEarlGrey signOut];

  [ChromeEarlGrey
      waitForSyncEngineInitialized:NO
                       syncTimeout:syncher::kSyncUKMOperationsTimeout];
  [ChromeEarlGrey clearFakeSyncServerData];

  [super tearDown];
}

#pragma mark - Helpers

// Waits for a new incognito tab to be opened.
- (void)openNewIncognitoTab {
  const NSUInteger incognitoTabCount = [ChromeEarlGrey incognitoTabCount];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:(incognitoTabCount + 1)];
  GREYAssert([ChromeEarlGrey isIncognitoMode],
             @"Failed to switch to incognito mode.");
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
             @"Failed to switch to normal mode.");
}

// Waits for a new tab to be opened.
- (void)openNewRegularTab {
  const NSUInteger tabCount = [ChromeEarlGrey mainTabCount];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:(tabCount + 1)];
}

#pragma mark - Tests

// The tests in this file should correspond to the tests in //chrome/browser/
// metrics/ukm_browsertest.cc.

// Make sure that UKM is disabled while an incognito tab is open.
//
// Corresponds to RegularPlusIncognitoCheck in //chrome/browser/metrics/
// ukm_browsertest.cc.
- (void)testRegularPlusIncognito {
  // Note: Tests begin with an open regular tab. This tab is opened in setUp.
  const uint64_t originalClientID = [MetricsAppInterface UKMClientID];

  [self openNewIncognitoTab];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  // Opening another regular tab mustn't enable UKM.
  [self openNewRegularTab];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  // Opening and closing an incognito tab mustn't enable UKM.
  [self openNewIncognitoTab];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");
  [self closeCurrentIncognitoTab];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  // Open a new regular tab to switch from incognito mode to normal mode. Then,
  // close this newly-opened regular tab plus the regular tab that was opened
  // after the first incognito tab was opened.
  //
  // TODO(crbug.com/41271925): Due to continuous animations, it is not feasible
  // (i) to use the tab switcher to switch between modes or (ii) to omit the
  // below code block and simply call [ChromeEarlGrey closeAllIncognitoTabs];
  // from incognito mode.
  [self openNewRegularTab];
  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey closeCurrentTab];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  // At this point, there is one open regular tab and one open incognito tab.
  [ChromeEarlGrey closeAllIncognitoTabs];

  // All incognito tabs have been closed, so UKM should be enabled.
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");

  // Client ID should not have been reset.
  GREYAssertEqual(originalClientID, [MetricsAppInterface UKMClientID],
                  @"Client ID was reset.");
}

// Make sure opening a real tab after Incognito doesn't enable UKM.
//
// Corresponds to IncognitoPlusRegularCheck in //chrome/browser/metrics/
// ukm_browsertest.cc.
- (void)testIncognitoPlusRegular {
  // Note: Tests begin with an open regular tab. This tab is opened in setUp.
  const uint64_t originalClientID = [MetricsAppInterface UKMClientID];

  // TODO(crbug.com/41271925): Due to continuous animations, it is not feasible
  // to close the regular tab that is already open. The functions closeAllTabs,
  // closeCurrentTab, and closeAllTabsInCurrentMode close the tab and then hang.
  //
  // As a workaround, we open an incognito tab and then close the regular tab to
  // get to a state in which a single incognito tab is open.
  [self openNewIncognitoTab];
  [ChromeEarlGrey closeAllNormalTabs];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  // Opening another regular tab mustn't enable UKM.
  [self openNewRegularTab];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  [ChromeEarlGrey closeAllIncognitoTabs];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");

  // Client ID should not have been reset.
  GREYAssertEqual(originalClientID, [MetricsAppInterface UKMClientID],
                  @"Client ID was reset.");
}

// testRegularPlusGuest is unnecessary since there can't be multiple profiles.

// testOpenNonSync is unnecessary since there can't be multiple profiles.

// Make sure that UKM is disabled when metrics consent is revoked.
//
// Corresponds to MetricsConsentCheck in //chrome/browser/metrics/
// ukm_browsertest.cc.
- (void)testMetricsConsent {
  const uint64_t originalClientID = [MetricsAppInterface UKMClientID];

  [MetricsAppInterface setMetricsAndCrashReportingForTesting:NO];

  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  [MetricsAppInterface setMetricsAndCrashReportingForTesting:YES];

  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");
  // Client ID should have been reset.
  GREYAssertNotEqual(originalClientID, [MetricsAppInterface UKMClientID],
                     @"Client ID was not reset.");
}

// The tests corresponding to AddSyncedUserBirthYearAndGenderToProtoData in
// //chrome/browser/metrics/ukm_browsertest.cc. are in demographics_egtest.mm.

// Make sure that providing metrics consent doesn't enable UKM when the user
// is signed-in but history sync is disabled.
- (void)testConsentAddedButNoHistorySync {
  [SigninEarlGrey signOut];
  [MetricsAppInterface setMetricsAndCrashReportingForTesting:NO];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  [MetricsAppInterface setMetricsAndCrashReportingForTesting:YES];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  // Once history sync is enabled, UKM is too.
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                         enableHistorySync:YES];
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");
}

// Make sure that UKM is disabled when "Make searches and browsing better" is
// disabled.
//
// Corresponds to ClientIdResetWhenConsentRemoved in //chrome/browser/metrics/
// ukm_browsertest.cc.
- (void)testClientIdResetWhenConsentRemoved {
  const uint64_t originalClientID = [MetricsAppInterface UKMClientID];

  [ChromeEarlGreyUI openSettingsMenu];
  // Open Google services settings.
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  // Toggle "Make searches and browsing better" switch off.

  [[[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   @"betterSearchAndBrowsingItem_switch", YES)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:chrome_test_util::GoogleServicesSettingsView()]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");

  // Toggle "Make searches and browsing better" switch on.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   @"betterSearchAndBrowsingItem_switch", NO)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");
  // Client ID should have been reset.
  GREYAssertNotEqual(originalClientID, [MetricsAppInterface UKMClientID],
                     @"Client ID was not reset.");

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Make sure that UKM is disabled when the user is signed out.
- (void)testSingleSignout {
  const uint64_t clientID1 = [MetricsAppInterface UKMClientID];

  [SigninEarlGrey signOut];

  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:NO],
             @"Failed to assert that UKM was not enabled.");
  // Client ID should have been reset by signout.
  GREYAssertNotEqual(clientID1, [MetricsAppInterface UKMClientID],
                     @"Client ID was not reset.");

  // UKM requires enabling history sync.
  const uint64_t clientID2 = [MetricsAppInterface UKMClientID];
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                         enableHistorySync:YES];

  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");
  // Client ID should not have been reset.
  GREYAssertEqual(clientID2, [MetricsAppInterface UKMClientID],
                  @"Client ID was reset.");
}

// testMultiSyncSignout is unnecessary since there can't be multiple profiles.

// testMetricsReporting is unnecessary since iOS doesn't use sampling.

// Tests that pending data is deleted when the user deletes their history.
//
// Corresponds to HistoryDeleteCheck in //chrome/browser/metrics/
// ukm_browsertest.cc.
- (void)testHistoryDelete {
  const uint64_t originalClientID = [MetricsAppInterface UKMClientID];

  const uint64_t kDummySourceId = 0x54321;
  [MetricsAppInterface UKMRecordDummySource:kDummySourceId];
  GREYAssert([MetricsAppInterface UKMHasDummySource:kDummySourceId],
             @"Dummy source failed to record.");

  [ChromeEarlGrey clearBrowsingHistory];
  GREYAssertEqual([ChromeEarlGrey browsingHistoryEntryCount], 0,
                  @"History was unexpectedly non-empty");

  // Other sources may have already been recorded since the data was cleared,
  // but the dummy source should be gone.
  GREYAssert(![MetricsAppInterface UKMHasDummySource:kDummySourceId],
             @"Dummy source was not purged.");
  GREYAssertEqual(originalClientID, [MetricsAppInterface UKMClientID],
                  @"Client ID was reset.");
  GREYAssert([MetricsAppInterface checkUKMRecordingEnabled:YES],
             @"Failed to assert that UKM was enabled.");
}

@end
