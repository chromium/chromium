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
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_egtest_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/sync_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::AccountsSyncButton;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ClearBrowsingDataCollectionView;
using chrome_test_util::GetIncognitoTabCount;
using chrome_test_util::IsIncognitoMode;
using chrome_test_util::IsSyncInitialized;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuPrivacyButton;
using chrome_test_util::SignOutAccountsButton;
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
    return service && base::ContainsKey(service->sources(), source_id);
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

void AssertSyncInitialized(bool is_initialized) {
  ConditionBlock condition = ^{
    return IsSyncInitialized() == is_initialized;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kSyncUKMOperationsTimeout, condition),
             @"Failed to assert whether Sync was initialized or not.");
}

void AssertUKMEnabled(bool is_enabled) {
  ConditionBlock condition = ^{
    return metrics::UkmEGTestHelper::ukm_enabled() == is_enabled;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kSyncUKMOperationsTimeout, condition),
             @"Failed to assert whether UKM was enabled or not.");
}

// Matcher for the Clear Browsing Data cell on the Privacy screen.
id<GREYMatcher> ClearBrowsingDataCell() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
}
// Matcher for the clear browsing data button on the clear browsing data panel.
id<GREYMatcher> ClearBrowsingDataButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CLEAR_BUTTON);
}

void ClearBrowsingData() {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI tapPrivacyMenuButton:ClearBrowsingDataCell()];
  [ChromeEarlGreyUI tapClearBrowsingDataMenuButton:ClearBrowsingDataButton()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          ConfirmClearBrowsingDataButton()]
      performAction:grey_tap()];

  // Before returning, make sure that the top of the Clear Browsing Data
  // settings screen is visible to match the state at the start of the method.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataCollectionView()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeTop)];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

void OpenNewIncognitoTab() {
  NSUInteger incognito_tab_count = GetIncognitoTabCount();
  chrome_test_util::OpenNewIncognitoTab();
  [ChromeEarlGrey waitForIncognitoTabCount:(incognito_tab_count + 1)];
  GREYAssert(IsIncognitoMode(), @"Failed to switch to incognito mode.");
}

void CloseCurrentIncognitoTab() {
  NSUInteger incognito_tab_count = GetIncognitoTabCount();
  chrome_test_util::CloseCurrentTab();
  [ChromeEarlGrey waitForIncognitoTabCount:(incognito_tab_count - 1)];
}

void CloseAllIncognitoTabs() {
  GREYAssert(chrome_test_util::CloseAllIncognitoTabs(), @"Tabs did not close");
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  // The user is dropped into the tab grid after closing the last incognito tab.
  // Therefore this test must manually switch back to showing the normal tabs.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
      performAction:grey_tap()];
  GREYAssert(!IsIncognitoMode(), @"Failed to switch to normal mode.");
}

void OpenNewRegularTab() {
  NSUInteger tab_count = chrome_test_util::GetMainTabCount();
  chrome_test_util::OpenNewTab();
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

  [SigninEarlGreyUtils assertSignedOut];
}

}  // namespace

// UKM tests.
@interface UKMTestCase : ChromeTestCase

@end

@implementation UKMTestCase

+ (void)setUp {
  [super setUp];
  if (!base::FeatureList::IsEnabled(ukm::kUkmFeature)) {
    // ukm::kUkmFeature feature is not enabled. You need to pass
    // --enable-features=Ukm command line argument in order to run this test.
    DCHECK(false);
  }
}

- (void)setUp {
  [super setUp];

  AssertSyncInitialized(false);
  AssertUKMEnabled(false);

  // Enable sync.
  [SigninEarlGreyUI signinWithIdentity:[SigninEarlGreyUtils fakeIdentity1]];
  AssertSyncInitialized(true);

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
  AssertSyncInitialized(true);
  AssertUKMEnabled(true);

  // Revoke metrics consent and update MetricsServicesManager.
  GREYAssert(g_metrics_enabled, @"Unpaired set/reset of user consent.");
  g_metrics_enabled = false;
  GetApplicationContext()->GetMetricsServicesManager()->UpdateUploadPermissions(
      true);
  IOSChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      nullptr);
  AssertUKMEnabled(false);

  // Disable sync.
  SignOut();
  AssertSyncInitialized(false);
  chrome_test_util::ClearSyncServerData();

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
  chrome_test_util::CloseAllTabs();
  [ChromeEarlGrey waitForMainTabCount:(0)];

  OpenNewIncognitoTab();
  AssertUKMEnabled(false);

  // Opening another regular tab mustn't enable UKM.
  OpenNewRegularTab();
  AssertUKMEnabled(false);

  GREYAssert(chrome_test_util::CloseAllIncognitoTabs(), @"Tabs did not close");
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
  // Open accounts settings, then sync settings.
  [[EarlGrey selectElementWithMatcher:SettingsAccountButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AccountsSyncButton()]
      performAction:grey_tap()];
  // Toggle "Sync Everything" then "History" switches off.
  [[EarlGrey selectElementWithMatcher:SyncSwitchCell(
                                          l10n_util::GetNSString(
                                              IDS_IOS_SYNC_EVERYTHING_TITLE),
                                          YES)]
      performAction:TurnSyncSwitchOn(NO)];
  [[EarlGrey
      selectElementWithMatcher:SyncSwitchCell(l10n_util::GetNSString(
                                                  IDS_SYNC_DATATYPE_TYPED_URLS),
                                              YES)]
      performAction:TurnSyncSwitchOn(NO)];

  AssertUKMEnabled(false);

  // Toggle "History" then "Sync Everything" switches on.
  [[EarlGrey
      selectElementWithMatcher:SyncSwitchCell(l10n_util::GetNSString(
                                                  IDS_SYNC_DATATYPE_TYPED_URLS),
                                              NO)]
      performAction:TurnSyncSwitchOn(YES)];
  [[EarlGrey selectElementWithMatcher:SyncSwitchCell(
                                          l10n_util::GetNSString(
                                              IDS_IOS_SYNC_EVERYTHING_TITLE),
                                          NO)]
      performAction:TurnSyncSwitchOn(YES)];

  AssertUKMEnabled(true);
  // Client ID should have been reset.
  GREYAssert(original_client_id != metrics::UkmEGTestHelper::client_id(),
             @"Client ID was not reset.");

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// testMultiDisableSync not needed, since there can't be multiple profiles.

// Make sure that UKM is disabled when a secondary passphrase is used.
- (void)testSecondaryPassphrase {
  uint64_t original_client_id = metrics::UkmEGTestHelper::client_id();

  [ChromeEarlGreyUI openSettingsMenu];
  // Open accounts settings, then sync settings.
  [[EarlGrey selectElementWithMatcher:SettingsAccountButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AccountsSyncButton()]
      performAction:grey_tap()];
  // Open sync encryption menu.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"kSettingsSyncId")]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          l10n_util::GetNSStringWithFixup(
                                              IDS_IOS_SYNC_ENCRYPTION_TITLE))]
      performAction:grey_tap()];
  // Select passphrase encryption.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_SYNC_FULL_ENCRYPTION_DATA)]
      performAction:grey_tap()];
  // Type and confirm passphrase, then submit.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityValue(@"Passphrase")]
      performAction:grey_replaceText(@"mypassphrase")];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityValue(@"Confirm passphrase")]
      performAction:grey_replaceText(@"mypassphrase")];

  AssertUKMEnabled(false);
  // Client ID should have been reset.
  GREYAssert(original_client_id != metrics::UkmEGTestHelper::client_id(),
             @"Client ID was not reset.");

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Reset sync back to original state.
  SignOut();
  chrome_test_util::ClearSyncServerData();
  [SigninEarlGreyUI signinWithIdentity:[SigninEarlGreyUtils fakeIdentity1]];
  AssertUKMEnabled(true);
}

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
