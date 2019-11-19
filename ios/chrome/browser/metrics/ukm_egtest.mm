// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/macros.h"
#include "base/stl_util.h"
#import "base/test/ios/wait_util.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/ukm_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_view_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::AccountsSyncButton;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ClearBrowsingDataButton;
using chrome_test_util::ClearBrowsingDataCell;
using chrome_test_util::ConfirmClearBrowsingDataButton;
using chrome_test_util::GoogleServicesSettingsButton;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuPrivacyButton;
using chrome_test_util::SyncSwitchCell;
using chrome_test_util::TurnSyncSwitchOn;

namespace metrics {

// Helper class that provides access to UKM internals.
class UkmEGTestHelper {
 public:
  UkmEGTestHelper() {}

  static bool ukm_enabled() {
    auto* service = ukm_service();
    return service ? service->recording_enabled_ : false;
  }

  static uint64_t client_id() {
    auto* service = ukm_service();
    return service ? service->client_id_ : 0;
  }

  static bool HasDummySource(ukm::SourceId source_id) {
    auto* service = ukm_service();
    return service && base::Contains(service->sources(), source_id);
  }

  static void RecordDummySource(ukm::SourceId source_id) {
    auto* service = ukm_service();
    if (service)
      service->UpdateSourceURL(source_id, GURL("http://example.com"));
  }

 private:
  static ukm::UkmService* ukm_service() {
    return GetApplicationContext()
        ->GetMetricsServicesManager()
        ->GetUkmService();
  }

  DISALLOW_COPY_AND_ASSIGN(UkmEGTestHelper);
};

}  // namespace metrics

namespace {

bool g_metrics_enabled = false;

// Constant for timeout while waiting for asynchronous sync and UKM operations.
const NSTimeInterval kSyncUKMOperationsTimeout = 10.0;

void AssertUKMEnabled(bool is_enabled) {
  ConditionBlock condition = ^{
    return metrics::UkmEGTestHelper::ukm_enabled() == is_enabled;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kSyncUKMOperationsTimeout, condition),
             @"Failed to assert whether UKM was enabled or not.");
}

void ClearBrowsingData() {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI tapPrivacyMenuButton:ClearBrowsingDataCell()];
  [ChromeEarlGreyUI tapClearBrowsingDataMenuButton:ClearBrowsingDataButton()];
  [[EarlGrey selectElementWithMatcher:ConfirmClearBrowsingDataButton()]
      performAction:grey_tap()];

  // Wait until activity indicator modal is cleared, meaning clearing browsing
  // data has been finished.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

void OpenNewIncognitoTab() {
  NSUInteger incognito_tab_count = [ChromeEarlGrey incognitoTabCount];
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey waitForIncognitoTabCount:(incognito_tab_count + 1)];
  GREYAssert([ChromeEarlGrey isIncognitoMode],
             @"Failed to switch to incognito mode.");
}

void CloseCurrentIncognitoTab() {
  NSUInteger incognito_tab_count = [ChromeEarlGrey incognitoTabCount];
  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey waitForIncognitoTabCount:(incognito_tab_count - 1)];
}

void CloseAllIncognitoTabs() {
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

void OpenNewRegularTab() {
  NSUInteger tab_count = [ChromeEarlGrey mainTabCount];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:(tab_count + 1)];
}

// Grant/revoke metrics consent and update MetricsServicesManager.
void UpdateMetricsConsent(bool new_state) {
  g_metrics_enabled = new_state;
  GetApplicationContext()->GetMetricsServicesManager()->UpdateUploadPermissions(
      true);
}

// Signs out of sync.
void SignOut() {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Remove |identity| from the device.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(identity.userEmail)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(@"Remove account")]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  [SigninEarlGreyUtils checkSignedOut];
}

}  // namespace

// UKM tests.
@interface UKMTestCase : ChromeTestCase

@end

@implementation UKMTestCase

+ (void)setUp {
  [super setUp];
  if (![ChromeEarlGrey isUKMEnabled]) {
    // ukm::kUkmFeature feature is not enabled. You need to pass
    // --enable-features=Ukm command line argument in order to run this test.
    DCHECK(false);
  }
}

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForSyncInitialized:NO
                             syncTimeout:kSyncUKMOperationsTimeout];
  AssertUKMEnabled(false);

  // Sign in to Chrome and turn sync on.
  //
  // Note: URL-keyed anonymized data collection is turned on as part of the
  // flow to Sign in to Chrome and Turn sync on. This matches the main user
  // flow that enables UKM.
  [SigninEarlGreyUI signinWithIdentity:[SigninEarlGreyUtils fakeIdentity1]];
  [ChromeEarlGrey waitForSyncInitialized:YES
                             syncTimeout:kSyncUKMOperationsTimeout];

  // Grant metrics consent and update MetricsServicesManager.
  GREYAssert(!g_metrics_enabled, @"Unpaired set/reset of user consent.");
  g_metrics_enabled = true;
  IOSChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &g_metrics_enabled);
  GetApplicationContext()->GetMetricsServicesManager()->UpdateUploadPermissions(
      true);
  AssertUKMEnabled(true);
}

- (void)tearDown {
  [ChromeEarlGrey waitForSyncInitialized:YES
                             syncTimeout:kSyncUKMOperationsTimeout];
  AssertUKMEnabled(true);

  // Revoke metrics consent and update MetricsServicesManager.
  GREYAssert(g_metrics_enabled, @"Unpaired set/reset of user consent.");
  g_metrics_enabled = false;
  GetApplicationContext()->GetMetricsServicesManager()->UpdateUploadPermissions(
      true);
  IOSChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      nullptr);
  AssertUKMEnabled(false);

  // Sign out of Chrome and Turn sync off.
  //
  // Note: URL-keyed anonymized data collection is turned off as part of the
  // flow to Sign out of Chrome and Turn sync off. This matchers the main user
  // flow that disables UKM.
  SignOut();
  [ChromeEarlGrey waitForSyncInitialized:NO
                             syncTimeout:kSyncUKMOperationsTimeout];
  [ChromeEarlGrey clearSyncServerData];

  [super tearDown];
}

// The tests in this file should correspond with the ones in
// //chrome/browser/metrics/ukm_browsertest.cc

// Make sure that UKM is disabled while an incognito tab is open.
- (void)testRegularPlusIncognito {
  uint64_t original_client_id = metrics::UkmEGTestHelper::client_id();

  OpenNewIncognitoTab();
  AssertUKMEnabled(false);

  // Opening another regular tab mustn't enable UKM.
  OpenNewRegularTab();
  AssertUKMEnabled(false);

  // Opening and closing an incognito tab mustn't enable UKM.
  OpenNewIncognitoTab();
  AssertUKMEnabled(false);
  CloseCurrentIncognitoTab();
  AssertUKMEnabled(false);

  CloseAllIncognitoTabs();
  AssertUKMEnabled(true);

  // Client ID should not have been reset.
  GREYAssert(original_client_id == metrics::UkmEGTestHelper::client_id(),
             @"Client ID was reset.");
}

// Make sure opening a real tab after Incognito doesn't enable UKM.
- (void)testIncognitoPlusRegular {
  uint64_t original_client_id = metrics::UkmEGTestHelper::client_id();
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey waitForMainTabCount:0];

  OpenNewIncognitoTab();
  AssertUKMEnabled(false);

  // Opening another regular tab mustn't enable UKM.
  OpenNewRegularTab();
  AssertUKMEnabled(false);

  [ChromeEarlGrey closeAllIncognitoTabs];
  [ChromeEarlGrey waitForIncognitoTabCount:0];
  AssertUKMEnabled(true);

  // Client ID should not have been reset.
  GREYAssert(original_client_id == metrics::UkmEGTestHelper::client_id(),
             @"Client ID was reset.");
}

// testOpenNonSync not needed, since there can't be multiple profiles.

// Make sure that UKM is disabled when metrics consent is revoked.
- (void)testMetricsConsent {
  uint64_t original_client_id = metrics::UkmEGTestHelper::client_id();

  UpdateMetricsConsent(false);

  AssertUKMEnabled(false);

  UpdateMetricsConsent(true);

  AssertUKMEnabled(true);
  // Client ID should have been reset.
  GREYAssert(original_client_id != metrics::UkmEGTestHelper::client_id(),
             @"Client ID was not reset.");
}

// Make sure that providing metrics consent doesn't enable UKM w/o sync.
- (void)testConsentAddedButNoSync {
  SignOut();
  UpdateMetricsConsent(false);
  AssertUKMEnabled(false);

  UpdateMetricsConsent(true);
  AssertUKMEnabled(false);

  [SigninEarlGreyUI signinWithIdentity:[SigninEarlGreyUtils fakeIdentity1]];
  AssertUKMEnabled(true);
}

// Make sure that UKM is disabled when sync is disabled.
- (void)testSingleDisableSync {
  uint64_t original_client_id = metrics::UkmEGTestHelper::client_id();

  [ChromeEarlGreyUI openSettingsMenu];
    // Open Sync and Google services settings
    [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
    // Toggle "Make searches and browsing better" switch off.

    [[[EarlGrey
        selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                     @"betterSearchAndBrowsingItem_switch",
                                     YES)]
           usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
        onElementWithMatcher:grey_accessibilityID(
                                 kGoogleServicesSettingsViewIdentifier)]
        performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];

  AssertUKMEnabled(false);

    // Toggle "Make searches and browsing better" switch on.
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                     @"betterSearchAndBrowsingItem_switch", NO)]
        performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  AssertUKMEnabled(true);
  // Client ID should have been reset.
  GREYAssert(original_client_id != metrics::UkmEGTestHelper::client_id(),
             @"Client ID was not reset.");

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// testMultiDisableSync not needed, since there can't be multiple profiles.

// Make sure that UKM is disabled when sync is not enabled.
- (void)testSingleSyncSignout {
  uint64_t original_client_id = metrics::UkmEGTestHelper::client_id();

  SignOut();

  AssertUKMEnabled(false);
  // Client ID should have been reset by signout.
  GREYAssert(original_client_id != metrics::UkmEGTestHelper::client_id(),
             @"Client ID was not reset.");

  original_client_id = metrics::UkmEGTestHelper::client_id();
  [SigninEarlGreyUI signinWithIdentity:[SigninEarlGreyUtils fakeIdentity1]];

  AssertUKMEnabled(true);
  // Client ID should not have been reset.
  GREYAssert(original_client_id == metrics::UkmEGTestHelper::client_id(),
             @"Client ID was reset.");
}

// testMultiSyncSignout not needed, since there can't be multiple profiles.

// testMetricsReporting not needed, since iOS doesn't use sampling.

// TODO(crbug.com/866598): Re-enable this test.
- (void)DISABLED_testHistoryDelete {
  uint64_t original_client_id = metrics::UkmEGTestHelper::client_id();

  const ukm::SourceId kDummySourceId = 0x54321;
  metrics::UkmEGTestHelper::RecordDummySource(kDummySourceId);
  GREYAssert(metrics::UkmEGTestHelper::HasDummySource(kDummySourceId),
             @"Dummy source failed to record.");

  ClearBrowsingData();

  // Other sources may already have been recorded since the data was cleared,
  // but the dummy source should be gone.
  GREYAssert(!metrics::UkmEGTestHelper::HasDummySource(kDummySourceId),
             @"Dummy source was not purged.");
  GREYAssert(original_client_id == metrics::UkmEGTestHelper::client_id(),
             @"Client ID was reset.");
  AssertUKMEnabled(true);
}

@end
