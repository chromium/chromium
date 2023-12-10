// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/bookmarks/common/storage_type.h"
#import "components/policy/policy_constants.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "components/sync/base/user_selectable_type.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/elements_constants.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_constants.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util_mac.h"

using chrome_test_util::BookmarksNavigationBarDoneButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::GoogleServicesSettingsButton;
using chrome_test_util::GoogleSyncSettingsButton;
using chrome_test_util::IdentityCellMatcherForEmail;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsCollectionView;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsImportDataContinueButton;
using chrome_test_util::SettingsImportDataImportButton;
using chrome_test_util::SettingsImportDataKeepSeparateButton;
using chrome_test_util::SettingsLink;
using chrome_test_util::SettingsSignInRowMatcher;
using chrome_test_util::StaticTextWithAccessibilityLabelId;
using l10n_util::GetNSString;
using l10n_util::GetNSStringF;
using testing::ButtonWithAccessibilityLabel;

typedef NS_ENUM(NSInteger, OpenSigninMethod) {
  OpenSigninMethodFromSettings,
  OpenPrimarySigninMethodFromBookmarks,
  OpenSecondarySigninMethodFromBookmarks,
  OpenSigninMethodFromRecentTabs,
  OpenSigninMethodFromTabSwitcher,
};

// TODO(crbug.com/1277545): Flaky on iOS simulator.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testSwipeDownInAdvancedSettings \
  DISABLED_testSwipeDownInAdvancedSettings
#else
#define MAYBE_testSwipeDownInAdvancedSettings testSwipeDownInAdvancedSettings
#endif
// TODO(crbug.com/1277545): Flaky on iOS simulator.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testSyncOnWhenPassphraseIntroducedAfterSignIn \
  DISABLED_testSyncOnWhenPassphraseIntroducedAfterSignIn
#else
#define MAYBE_testSyncOnWhenPassphraseIntroducedAfterSignIn \
  testSyncOnWhenPassphraseIntroducedAfterSignIn
#endif

namespace {

// Label used to find the 'Learn more' link.
NSString* const kLearnMoreLabel = @"Learn More";

// Text displayed in the chrome://management page.
char const kManagedText[] = "Your browser is managed by your administrator.";

NSString* const kPassphrase = @"hello";

// Timeout in seconds to wait for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(5);

// Sets parental control capability for the given identity.
void SetParentalControlsCapabilityForIdentity(FakeSystemIdentity* identity) {
  // The identity must exist in the test storage to be able to set capabilities
  // through the fake identity service.
  [SigninEarlGrey addFakeIdentity:identity];
  [SigninEarlGrey setIsSubjectToParentalControls:YES forIdentity:identity];
}
// Closes the sign-in import data dialog and choose either to combine the data
// or keep the data separate.
void CloseImportDataDialog(id<GREYMatcher> choiceButtonMatcher) {
  // Select the import data choice.
  [[EarlGrey selectElementWithMatcher:choiceButtonMatcher]
      performAction:grey_tap()];

  // Close the import data dialog.
  [[EarlGrey selectElementWithMatcher:SettingsImportDataContinueButton()]
      performAction:grey_tap()];
}

// Signs in with one account, signs out, and then signs in with a second
// account.
void ChooseImportOrKeepDataSepareteDialog(id<GREYMatcher> choiceButtonMatcher) {
  // Set up the fake identities.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // Sign in to `fakeIdentity1`.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceKeepData];

  // Sign in with `fakeIdentity2`.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsSignInRowMatcher()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      performAction:grey_tap()];
  [SigninEarlGreyUI tapSigninConfirmationDialog];

  // Switch Sync account to `fakeIdentity2` should ask whether date should be
  // imported or kept separate. Choose to keep data separate.
  CloseImportDataDialog(choiceButtonMatcher);

  // Check the signed-in user did change.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity2];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

void ExpectSigninConsentHistogram(
    signin_metrics::SigninAccountType signinAccountType) {
  NSError* error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@"Signin.AccountType.SigninConsent"];
  GREYAssertNil(error, @"Failed to record show count histogram");
  error = [MetricsAppInterface expectCount:1
                                 forBucket:static_cast<int>(signinAccountType)
                              forHistogram:@"Signin.AccountType.SigninConsent"];
  GREYAssertNil(error, @"Failed to record show count histogram");
}

void ExpectNoSyncConsentHistogram(
    signin_metrics::SigninAccountType signinAccountType) {
  NSError* error =
      [MetricsAppInterface expectTotalCount:0
                               forHistogram:@"Signin.AccountType.SyncConsent"];
  GREYAssertNil(error, @"Failed to record show count histogram");
  error = [MetricsAppInterface expectCount:0
                                 forBucket:static_cast<int>(signinAccountType)
                              forHistogram:@"Signin.AccountType.SyncConsent"];
  GREYAssertNil(error, @"Failed to record show count histogram");
}

// Sets up the sign-in policy value dynamically at runtime.
void SetSigninEnterprisePolicyValue(BrowserSigninMode signinMode) {
  policy_test_utils::SetPolicy(static_cast<int>(signinMode),
                               policy::key::kBrowserSignin);
}

}  // namespace

// Sign-in interaction tests that work both with Unified Consent enabled or
// disabled.
@interface SigninCoordinatorTestCase : WebHttpServerChromeTestCase
@end

@implementation SigninCoordinatorTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self isRunningTest:@selector
            (testSignInSwitchAccountsAndKeepDataSeparate)] ||
      [self isRunningTest:@selector(testSignInSwitchAccountsAndImportData)] ||
      [self isRunningTest:@selector(testSignInCancelIdentityPicker)] ||
      [self isRunningTest:@selector(testSignInCancelAuthenticationFlow)] ||
      [self isRunningTest:@selector
            (testDismissAdvancedSigninSettingsFromAdvancedSigninSettings)] ||
      [self isRunningTest:@selector
            (testDismissSigninFromRecentTabsFromAdvancedSigninSettings)] ||
      [self isRunningTest:@selector
            (testDismissSigninFromTabSwitcherFromAdvancedSigninSettings)] ||
      [self isRunningTest:@selector(testSignInCancelAddAccount)] ||
      [self isRunningTest:@selector(testSignInFromSyncOffLink)] ||
      [self isRunningTest:@selector
            (testSignInWithOneAccountStartSyncWithAnotherAccount)] ||
      [self isRunningTest:@selector(testSyncTypesDisabledPolicy)] ||
      [self
          isRunningTest:@selector(testSwipeDownSignInViewWithoutAnIdentity)] ||
      [self isRunningTest:@selector(MAYBE_testSwipeDownInAdvancedSettings)] ||
      [self isRunningTest:@selector
            (MAYBE_testSyncOnWhenPassphraseIntroducedAfterSignIn)] ||
      [self isRunningTest:@selector(testCancelFromSyncOffLink)] ||
      [self
          isRunningTest:@selector
          (testPrimaryAccountLabelUpdate_ReplaceSyncPromosWithSignInPromosDisabled
              )]) {
    // TODO(crbug.com/1477295): Evaluate if these tests are relevant with
    // kReplaceSyncPromosWithSignInPromos enabled.
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }
  if ([self isRunningTest:@selector(testOpenSignInFromNTP)] ||
      [self isRunningTest:@selector(testSignInFromSettingsMenu)] ||
      [self isRunningTest:@selector
            (testSignOutForSupervisedUserClearAccountData)] ||
      [self isRunningTest:@selector
            (testDismissSigninFromTabSwitcherFromIdentityPicker)] ||
      [self isRunningTest:@selector(testDismissSigninFromTabSwitcher)] ||
      [self isRunningTest:@selector
            (testOpenSigninSheetFromNTPIfHasDeviceAccount)] ||
      [self isRunningTest:@selector
            (testOpenManageAddAccountFromNTPWhenSyncDisabledByPolicy)] ||
      [self isRunningTest:@selector
            (testOpenAuthActivityFromNTPIfNoDeviceAccount)] ||
      [self isRunningTest:@selector
            (testOpenSignInFromNTPWhenSyncDisabledByPolicy)] ||
      [self isRunningTest:@selector(testOpeningAddAccountView)] ||
      [self isRunningTest:@selector(testSwitchToSupervisedUser)] ||
      [self isRunningTest:@selector(testSigninDisabledByPolicy)] ||
      [self isRunningTest:@selector(testSignInFromNTPAndDeclineHistorySync)] ||
      [self isRunningTest:@selector(testSignInFromNTPAndAcceptHistorySync)] ||
      [self isRunningTest:@selector(testSignInDisconnectFromChromeManaged)] ||
      [self isRunningTest:@selector(testSignInOneUser)]) {
    config.features_enabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }

  return config;
}

- (void)setUp {
  [super setUp];
  // Remove closed tab history to make sure the sign-in promo is always visible
  // in recent tabs.
  [ChromeEarlGrey clearBrowsingHistory];
  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [BookmarkEarlGrey clearBookmarks];
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to set up histogram tester.");
}

- (void)tearDown {
  [super tearDown];
  [BookmarkEarlGrey clearBookmarksPositionCache];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
}

// Tests that opening the sign-in screen from the Settings and signing in works
// correctly when there is already an identity on the device.
- (void)testSignInOneUser {
  // Set up a fake identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Check `fakeIdentity` is signed-in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  ExpectSigninConsentHistogram(signin_metrics::SigninAccountType::kRegular);
  ExpectNoSyncConsentHistogram(signin_metrics::SigninAccountType::kRegular);
}

// Tests that opening the sign-in screen from the Settings and signing in works
// correctly when there is a supervised user identity on the device.
- (void)testSignInSupervisedUser {
  // Set up a fake supervised identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  SetParentalControlsCapabilityForIdentity(fakeIdentity);

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that switching between an unsupervised and supervised account does not
// merge data on the device.
- (void)testSwitchToSupervisedUser {
  // Add a fake supervised identity to the device.
  FakeSystemIdentity* fakeSupervisedIdentity =
      [FakeSystemIdentity fakeIdentity1];
  SetParentalControlsCapabilityForIdentity(fakeSupervisedIdentity);

  // Add a fake identity to the device.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Sign in with fake identity.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];

  // Confirmation choice is ignored when `kReplaceSyncPromosWithSignInPromos` is
  // enabled.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceKeepData];

  // Sign in with fake supervised identity.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeSupervisedIdentity];

  // Verify bookmarks are cleared.
  [BookmarkEarlGreyUI openBookmarks];
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_text(GetNSString(
                                            IDS_IOS_BOOKMARK_EMPTY_TITLE))]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Waiting for bookmarks to be cleared");
}

// Tests that signing out a supervised user account with the keep local data
// option is honored.
- (void)testSignOutWithKeepDataForSupervisedUser {
  // Sign in with a fake supervised identity.
  FakeSystemIdentity* fakeSupervisedIdentity =
      [FakeSystemIdentity fakeIdentity1];
  SetParentalControlsCapabilityForIdentity(fakeSupervisedIdentity);
  [SigninEarlGreyUI signinWithFakeIdentity:fakeSupervisedIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];

  // Sign out from the supervised account with option to keep local data.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceKeepData];

  // Verify bookmarks are available.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI verifyEmptyBackgroundIsAbsent];
}

// Tests that signing out a supervised user account clears the account data.
// kReplaceSyncPromosWithSignInPromos is enabled.
- (void)testSignOutForSupervisedUserClearAccountData {
  // Sign in with a fake supervised identity.
  FakeSystemIdentity* fakeSupervisedIdentity =
      [FakeSystemIdentity fakeIdentity1];
  SetParentalControlsCapabilityForIdentity(fakeSupervisedIdentity);
  [SigninEarlGreyUI signinWithFakeIdentity:fakeSupervisedIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kAccount];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Sign out from the supervised account with option to clear local data.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceClearData];

  // Verify bookmarks are cleared.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI verifyEmptyBackgroundAppears];
}

// Tests signing in with one account, switching sync account to a second and
// choosing to keep the browsing data separate during the switch.
// kReplaceSyncPromosWithSignInPromos is disabled.
- (void)testSignInSwitchAccountsAndKeepDataSeparate {
  ChooseImportOrKeepDataSepareteDialog(SettingsImportDataKeepSeparateButton());
}

// Tests signing in with one account, switching sync account to a second and
// choosing to import the browsing data during the switch.
// kReplaceSyncPromosWithSignInPromos is disabled.
- (void)testSignInSwitchAccountsAndImportData {
  ChooseImportOrKeepDataSepareteDialog(SettingsImportDataImportButton());
}

// Tests that signing out from the Settings works correctly.
// TODO(crbug.com/1477295): Evaluate if the test is relevant with
// kReplaceSyncPromosWithSignInPromos enabled.
- (void)testSignInDisconnectFromChrome {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Sign out.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceKeepData];
}

// Tests that signing out of a managed account from the Settings works
// correctly.
- (void)testSignInDisconnectFromChromeManaged {
  // Sign-in with a managed account.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  ExpectSigninConsentHistogram(signin_metrics::SigninAccountType::kManaged);
  ExpectNoSyncConsentHistogram(signin_metrics::SigninAccountType::kManaged);

  // Sign out.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceClearData];
}

// Opens the sign in screen and then cancel it by opening a new tab. Ensures
// that the sign in screen is correctly dismissed. crbug.com/462200
// kReplaceSyncPromosWithSignInPromos is disabled.
// TODO(crbug.com/1477295): Evaluate if the test is relevant with
// kReplaceSyncPromosWithSignInPromos enabled.
- (void)testSignInCancelIdentityPicker {
  // Add an identity to avoid arriving on the Add Account screen when opening
  // sign-in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:NO];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  // Waits until the UI is fully presented before opening an URL.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Open new tab to cancel sign-in.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:NO];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity.userEmail)]
      performAction:grey_tap()];

  // Verifies that the Chrome sign-in view is visible.
  id<GREYMatcher> signin_matcher = StaticTextWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close sign-in screen and Settings.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Starts an authentication flow and cancel it by opening a new tab. Ensures
// that the authentication flow is correctly canceled and dismissed.
// crbug.com/462202
// kReplaceSyncPromosWithSignInPromos is disabled.
// TODO(crbug.com/1477295): Evaluate if the test is relevant with
// kReplaceSyncPromosWithSignInPromos enabled.
- (void)testSignInCancelAuthenticationFlow {
  // Set up the fake identities.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // This signs in `fakeIdentity2` first, ensuring that the "Clear Data Before
  // Syncing" dialog is shown during the second sign-in. This dialog will
  // effectively block the authentication flow, ensuring that the authentication
  // flow is always still running when the sign-in is being cancelled.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity2];

  // Sign out.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceKeepData];
  // Sign in with `fakeIdentity1`.
  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:NO];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity1.userEmail)]
      performAction:grey_tap()];

  // The authentication flow is only created when the confirm button is
  // selected. Note that authentication flow actually blocks as the
  // "Clear Browsing Before Syncing" dialog is presented.
  [SigninEarlGreyUI tapSigninConfirmationDialog];
  // Waits until the merge/delete data panel is shown.
  [[EarlGrey selectElementWithMatcher:SettingsImportDataKeepSeparateButton()]
      assertWithMatcher:grey_interactable()];

  // Open new tab to cancel sign-in.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:NO];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity1.userEmail)]
      performAction:grey_tap()];

  // Verifies that the Chrome sign-in view is visible.
  id<GREYMatcher> signin_matcher = StaticTextWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close sign-in screen and Settings.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];
}

#pragma mark - Dismiss tests

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: setting menu.
// Interrupted at: user consent.
- (void)testDismissSigninFromSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromSettings
                        tapSettingsLink:NO];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: setting menu.
// Interrupted at: advanced sign-in.
// kReplaceSyncPromosWithSignInPromos is disabled.
- (void)testDismissAdvancedSigninSettingsFromAdvancedSigninSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromSettings
                        tapSettingsLink:YES];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: recent tabs.
// Interrupted at: user consent.
- (void)testDismissSigninFromRecentTabs {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromRecentTabs
                        tapSettingsLink:NO];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: recent tabs.
// Interrupted at: advanced sign-in.
// kReplaceSyncPromosWithSignInPromos is disabled.
- (void)testDismissSigninFromRecentTabsFromAdvancedSigninSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromRecentTabs
                        tapSettingsLink:YES];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: tab switcher.
// Interrupted at: user consent.
- (void)testDismissSigninFromTabSwitcher {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromTabSwitcher
                        tapSettingsLink:NO];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: tab switcher.
// Interrupted at: advanced sign-in.
// kReplaceSyncPromosWithSignInPromos is disabled.
- (void)testDismissSigninFromTabSwitcherFromAdvancedSigninSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromTabSwitcher
                        tapSettingsLink:YES];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: tab switcher.
// Interrupted at: identity picker.
- (void)testDismissSigninFromTabSwitcherFromIdentityPicker {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [self openSigninFromView:OpenSigninMethodFromTabSwitcher tapSettingsLink:NO];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];

  // Open the URL as if it was opened from another app.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];

  [SigninEarlGrey verifySignedOut];
}

// Opens the reauth dialog and interrupts it by open an URL from an external
// app.
- (void)testInterruptReauthSignin {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyAppInterface triggerReauthDialogWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Open the URL as if it was opened from another app.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];
}

// Verifies that the user is signed in when selecting "Yes I'm In", after the
// advanced settings were swiped to dismiss.
// kReplaceSyncPromosWithSignInPromos is disabled.
- (void)MAYBE_testSwipeDownInAdvancedSettings {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:YES];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  [SigninEarlGreyUI tapSigninConfirmationDialog];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

#pragma mark - Utils

// Opens sign-in view.
// `openSigninMethod` is the way to start the sign-in.
// `tapSettingsLink` if YES, the setting link is tapped before opening the URL.
- (void)openSigninFromView:(OpenSigninMethod)openSigninMethod
           tapSettingsLink:(BOOL)tapSettingsLink {
  switch (openSigninMethod) {
    case OpenSigninMethodFromSettings:
      [ChromeEarlGreyUI openSettingsMenu];
      [ChromeEarlGreyUI tapSettingsMenuButton:SettingsSignInRowMatcher()];
      break;
    case OpenPrimarySigninMethodFromBookmarks:
      [ChromeEarlGreyUI openToolsMenu];
      [ChromeEarlGreyUI
          tapToolsMenuButton:chrome_test_util::BookmarksDestinationButton()];
      [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
          performAction:grey_tap()];
      break;
    case OpenSecondarySigninMethodFromBookmarks:
      [ChromeEarlGreyUI openToolsMenu];
      [ChromeEarlGreyUI
          tapToolsMenuButton:chrome_test_util::BookmarksDestinationButton()];
      [[EarlGrey selectElementWithMatcher:SecondarySignInButton()]
          performAction:grey_tap()];
      break;
    case OpenSigninMethodFromRecentTabs:
      [SigninEarlGreyUI tapPrimarySignInButtonInRecentTabs];
      break;
    case OpenSigninMethodFromTabSwitcher:
      [SigninEarlGreyUI tapPrimarySignInButtonInTabSwitcher];
      break;
  }
  if (tapSettingsLink) {
    [ChromeEarlGreyUI waitForAppToIdle];
    [[EarlGrey selectElementWithMatcher:SettingsLink()]
        performAction:grey_tap()];
  }
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Starts the sign-in workflow, and simulates opening an URL from another app.
// `openSigninMethod` is the way to start the sign-in.
// `tapSettingsLink` if YES, the setting link is tapped before opening the URL.
- (void)assertOpenURLWhenSigninFromView:(OpenSigninMethod)openSigninMethod
                        tapSettingsLink:(BOOL)tapSettingsLink {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [self openSigninFromView:openSigninMethod tapSettingsLink:tapSettingsLink];
  // Open the URL as if it was opened from another app.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];
  // Should be not signed in, after being interrupted.
  [SigninEarlGrey verifySignedOut];
}

// Checks that the fake SSO screen shown on adding an account is visible
// onscreen.
- (void)assertFakeSSOScreenIsVisible {
  // Check for the fake SSO screen.
  [ChromeEarlGrey
      waitForMatcher:grey_accessibilityID(kFakeAuthActivityViewIdentifier)];
  // Close the SSO view controller.
  id<GREYMatcher> matcher =
      grey_allOf(grey_accessibilityID(kFakeAuthCancelButtonIdentifier),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  // Make sure the SSO view controller is fully removed before ending the test.
  // The tear down needs to remove other view controllers, and it cannot be done
  // during the animation of the SSO view controler.
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Tests that the "add account" view can be opened from settings.
- (void)testOpeningAddAccountView {
  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:NO];
  [ChromeEarlGreyUI waitForAppToIdle];

  [self assertFakeSSOScreenIsVisible];
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Tests that an add account operation triggered from the web is handled.
// Regression test for crbug.com/1054861.
- (void)testSigninAddAccountFromWeb {
  [ChromeEarlGrey simulateAddAccountFromWeb];

  [self assertFakeSSOScreenIsVisible];
}

// Tests to remove the last identity in the identity chooser.
- (void)testRemoveLastAccount {
  // Set up a fake identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Open the identity chooser.
  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:NO];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForMatcher:IdentityCellMatcherForEmail(fakeIdentity.userEmail)];

  // Remove the fake identity.
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];

  // Check that the identity has been removed.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity.userEmail)]
      assertWithMatcher:grey_notVisible()];
}

// Opens the add account screen and then cancels it by opening a new tab.
// Ensures that the add account screen is correctly dismissed. crbug.com/462200
// kReplaceSyncPromosWithSignInPromos is disabled.
// TODO(crbug.com/1477295): Evaluate if the test is relevant with
// kReplaceSyncPromosWithSignInPromos enabled.
- (void)testSignInCancelAddAccount {
  // Add an identity to avoid arriving on the Add Account screen when opening
  // sign-in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:NO];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];

  // Open Add Account screen.
  id<GREYMatcher> add_account_matcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_ACCOUNT_IDENTITY_CHOOSER_ADD_ACCOUNT);
  [[EarlGrey selectElementWithMatcher:add_account_matcher]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Open new tab to cancel sign-in.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:NO];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity.userEmail)]
      performAction:grey_tap()];

  // Verifies that the Chrome sign-in view is visible.
  id<GREYMatcher> signin_matcher = StaticTextWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close sign-in screen and Settings.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Simulates a potential race condition in which the account is invalidated
// after the user taps the Settings button to navigate to the identity
// choosing UI. Depending on the timing, the account removal may occur after
// the UI has retrieved the list of valid accounts. The test ensures that in
// this case no account is presented to the user.
- (void)testAccountInvalidatedDuringSignin {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:NO];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];

  // Invalidate account after menu generation. If the underlying code does not
  // handle the race condition of removing an identity while showing menu is in
  // progress this test will likely be flaky.
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
  // Check that the identity has been removed.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity.userEmail)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the sign-in coordinator isn't started when sign-in is disabled by
// policy.
// kReplaceSyncPromosWithSignInPromos is disabled.
- (void)testSigninDisabledByPolicy {
  // Disable browser sign-in only after the "Sign in to Chrome" button is
  // visible.
  [ChromeEarlGreyUI openSettingsMenu];

  // Disable sign-in with policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kDisabled);

  // Verify the sign-in view isn't showing.
  id<GREYMatcher> signin_matcher = StaticTextWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_notVisible()];
}

// Tests that a signed-in user can open "Settings" screen from the NTP.
- (void)testOpenManageSyncSettingsFromNTP {
  // Sign in to Chrome.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Select the identity disc particle.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(GetNSStringF(
                                   IDS_IOS_IDENTITY_DISC_WITH_NAME_AND_EMAIL,
                                   base::SysNSStringToUTF16(
                                       fakeIdentity.userFullName),
                                   base::SysNSStringToUTF16(
                                       fakeIdentity.userEmail)))]
      performAction:grey_tap()];

  // Ensure the Settings menu is displayed.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the accessibility string for a signed-in user whose name is not
// available yet.
- (void)testAccessiblityStringForSignedInUserWithoutName {
  NSString* email = @"test@test.com";
  NSString* gaiaID = @"gaiaID";
  // Sign in to Chrome.
  FakeSystemIdentity* fakeIdentity =
      [FakeSystemIdentity identityWithEmail:email gaiaID:gaiaID name:nil];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Select the identity disc particle with the correct accessibility string.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(GetNSStringF(
                                          IDS_IOS_IDENTITY_DISC_WITH_EMAIL,
                                          base::SysNSStringToUTF16(
                                              fakeIdentity.userEmail)))]
      performAction:grey_tap()];

  // Ensure the Settings menu is displayed.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that a signed-in user can open "Settings" screen from the NTP.
- (void)testOpenManageSyncSettingsFromNTPWhenSigninIsNotAllowedByPolicy {
  // Disable sign-in with policy.
  SetSigninEnterprisePolicyValue(BrowserSigninMode::kDisabled);

  // Select the identity disc particle.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kNTPFeedHeaderIdentityDisc)]
      performAction:grey_tap()];

  // Ensure the Settings menu is displayed.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that a signed-in user can open "Settings" screen from the NTP.
- (void)testOpenManageAddAccountFromNTPWhenSyncDisabledByPolicy {
  // Disable sync by policy.
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(GetNSString(
                                       IDS_IOS_SYNC_SYNC_DISABLED_CONTINUE)),
                                   grey_userInteractionEnabled(), nil)]
      performAction:grey_tap()];

  // Select the identity disc particle.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kNTPFeedHeaderIdentityDisc)]
      performAction:grey_tap()];

  // Ensure the fake add-account menu is displayed. The existence of the "add
  // account" accessibility button on screen verifies that the screen
  // was shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthAddAccountButtonIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Tests that a signed-out user can open "Sign in and sync" screen from the NTP.
- (void)testOpenSignInFromNTP {
  // Select the identity disc particle.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(GetNSString(
                     IDS_IOS_IDENTITY_DISC_SIGNED_OUT_ACCESSIBILITY_LABEL))]
      performAction:grey_tap()];

  // Ensure the fake add-account menu is displayed. The existence of the "add
  // account" accessibility button on screen verifies that the screen
  // was shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthAddAccountButtonIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Tests that a signed-out user with device accounts can open "Sign in" sheet
// from the NTP.
- (void)testOpenSigninSheetFromNTPIfHasDeviceAccount {
  [SigninEarlGrey addFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Select the identity disc particle.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(GetNSString(
                     IDS_IOS_IDENTITY_DISC_SIGNED_OUT_ACCESSIBILITY_LABEL))]
      performAction:grey_tap()];

  // Ensure the sign-in sheet is displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_IDENTITY_DISC_SIGN_IN_PROMO_LABEL))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that when signing-in using the NTP avatar disc, the user is not signed
// out if history sync is declined.
- (void)testSignInFromNTPAndDeclineHistorySync {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Select the NTP avatar disc.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(GetNSString(
                     IDS_IOS_IDENTITY_DISC_SIGNED_OUT_ACCESSIBILITY_LABEL))]
      performAction:grey_tap()];

  // Confirm sign in.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kWebSigninPrimaryButtonAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the footer is shown without the user's email.
  id<GREYMatcher> footerTextMatcher = grey_allOf(
      grey_text(
          l10n_util::GetNSString(IDS_IOS_HISTORY_SYNC_FOOTER_WITHOUT_EMAIL)),
      grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:footerTextMatcher]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      assertWithMatcher:grey_notNil()];

  // Decline History Sync.
  [[[EarlGrey selectElementWithMatcher:
                  chrome_test_util::SigninScreenPromoSecondaryButtonMatcher()]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier)];

  // Verify that the history sync is disabled.
  GREYAssertFalse(
      [SigninEarlGreyAppInterface
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be disabled.");
  GREYAssertFalse([SigninEarlGreyAppInterface
                      isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
                  @"Tabs sync should be disabled.");
  // Verify that the identity is still signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that accepting History Sync after tapping on the NTP avatar disc while
// signed-out enables sync for tabs and history.
- (void)testSignInFromNTPAndAcceptHistorySync {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Select the NTP avatar disc.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(GetNSString(
                     IDS_IOS_IDENTITY_DISC_SIGNED_OUT_ACCESSIBILITY_LABEL))]
      performAction:grey_tap()];

  // Confirm sign in.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kWebSigninPrimaryButtonAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Accept History Sync.
  [[[EarlGrey selectElementWithMatcher:
                  chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier)];

  // Verify that the history sync is enabled.
  GREYAssertTrue(
      [SigninEarlGreyAppInterface
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be enabled.");
  GREYAssertTrue([SigninEarlGreyAppInterface
                     isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
                 @"Tabs sync should be enabled.");
  // Verify that the identity is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that a signed-out user with no device account can open the auth
// activity from the NTP.
- (void)testOpenAuthActivityFromNTPIfNoDeviceAccount {
  // Select the identity disc particle.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(GetNSString(
                     IDS_IOS_IDENTITY_DISC_SIGNED_OUT_ACCESSIBILITY_LABEL))]
      performAction:grey_tap()];

  // Ensure the auth activity is displayed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthActivityViewIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that a signed-out user with the SyncDisabled policy can still open the
// "Sign in" sheet from the NTP, to get sign-in benefits other than sync.
- (void)testOpenSignInFromNTPWhenSyncDisabledByPolicy {
  [SigninEarlGrey addFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  // Disable sync by policy.
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(GetNSString(
                                       IDS_IOS_SYNC_SYNC_DISABLED_CONTINUE)),
                                   grey_userInteractionEnabled(), nil)]
      performAction:grey_tap()];

  // Select the identity disc particle.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(GetNSString(
                     IDS_IOS_IDENTITY_DISC_SIGNED_OUT_ACCESSIBILITY_LABEL))]
      performAction:grey_tap()];

  // Ensure the sign-in sheet is displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kWebSigninPrimaryButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that opening the sign-in screen from the Settings and signing in works
// correctly when there is already an identity on the device.
// kReplaceSyncPromosWithSignInPromos is enabled.
- (void)testSignInFromSettingsMenu {
  // Set up a fake identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Check `fakeIdentity` is signed-in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Check the Settings Menu labels.
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsGoogleServicesCellId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SettingsAccountButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that opening the sign-in screen from the Sync Off tab and signin in
// will turn Sync On.
// kReplaceSyncPromosWithSignInPromos is disabled, as this test is about sync.
- (void)testSignInFromSyncOffLink {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  [ChromeEarlGreyUI openSettingsMenu];
  // Check Sync Off label is visible and user is signed in.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityValue(
                                       GetNSString(IDS_IOS_SETTING_OFF)),
                                   grey_accessibilityID(
                                       kSettingsGoogleSyncAndServicesCellId),
                                   nil)] performAction:grey_tap()];

  [SigninEarlGreyUI tapSigninConfirmationDialog];

  // Check Sync On label is visible and user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityValue(
                                       GetNSString(IDS_IOS_SETTING_ON)),
                                   grey_accessibilityID(
                                       kSettingsGoogleSyncAndServicesCellId),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that opening the sign-in screen from the Sync Off tab and canceling the
// sign-in flow will leave a signed-in with sync off user in the same state.
// kReplaceSyncPromosWithSignInPromos is disabled.
// TODO(crbug.com/1477295): Evaluate if the test is relevant with
// kReplaceSyncPromosWithSignInPromos enabled.
- (void)testCancelFromSyncOffLink {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  [ChromeEarlGreyUI openSettingsMenu];
  // Check Sync Off label is visible and user is signed in.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityValue(
                                       GetNSString(IDS_IOS_SETTING_OFF)),
                                   grey_accessibilityID(
                                       kSettingsGoogleSyncAndServicesCellId),
                                   nil)] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];

  // Check Sync Off label is visible and user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityValue(
                                       GetNSString(IDS_IOS_SETTING_OFF)),
                                   grey_accessibilityID(
                                       kSettingsGoogleSyncAndServicesCellId),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that the sign-in promo for no identities is displayed in Bookmarks when
// the user is signed out and has not added any identities to the device.
- (void)testSigninPromoWithNoIdentitiesOnDevice {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::BookmarksDestinationButton()];

  [SigninEarlGrey verifySignedOut];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];
}

// Tests that the sign-in promo with user name is displayed in Bookmarks when
// the user is signed out.
- (void)testSigninPromoWhenSignedOut {
  // Add identity to the device.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::BookmarksDestinationButton()];

  [SigninEarlGrey verifySignedOut];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];
}

// Tests that the sign-in promo is removed from Bookmarks when the user
// is signed out and has closed the sign-in promo with user name.
- (void)testSigninPromoClosedWhenSignedOut {
  // Add identity to the device.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::BookmarksDestinationButton()];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount
                           closeButton:YES];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSigninPromoCloseButtonId),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedOut];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that Sync is on when introducing passphrase from settings, after
// logging in.
// kReplaceSyncPromosWithSignInPromos is disabled.
// TODO(crbug.com/1477295): Evaluate if the test is relevant with
// kReplaceSyncPromosWithSignInPromos enabled.
- (void)MAYBE_testSyncOnWhenPassphraseIntroducedAfterSignIn {
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:NO];
  [SigninEarlGreyUI tapSigninConfirmationDialog];

  // Give the Sync state a chance to finish UI updates.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      GoogleServicesSettingsButton()];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityValue(GetNSString(
                                       IDS_IOS_SYNC_ENCRYPTION_DESCRIPTION)),
                                   grey_accessibilityID(
                                       kSettingsGoogleSyncAndServicesCellId),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Scroll to bottom of Manage Sync Settings, if necessary.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kManageSyncTableViewAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Select Encryption item.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_MANAGE_SYNC_ENCRYPTION)]
      performAction:grey_tap()];

  // Type and submit the sync passphrase.
  [SigninEarlGreyUI submitSyncPassphrase:kPassphrase];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SettingsDoneButton()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI openSettingsMenu];

  // Check Sync On label is visible.
  [SigninEarlGrey verifySyncUIEnabled:YES];
}

// Tests to sign-in with one user, and then turn on syncn with a second account.
// kReplaceSyncPromosWithSignInPromos is disabled as it is sync specific.
- (void)testSignInWithOneAccountStartSyncWithAnotherAccount {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // Sign-in only with fakeIdentity1.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1 enableSync:NO];

  // Open turn on sync dialog.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleSyncSettingsButton()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  // Select fakeIdentity2.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      performAction:grey_tap()];
  [SigninEarlGreyUI tapSigninConfirmationDialog];

  // Check fakeIdentity2 is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity2];
}

// Tests that when the syncTypesListDisabled policy is enabled, a policy warning
// is displayed with a link to the policy management page.
// kReplaceSyncPromosWithSignInPromos is disabled, because on sign-in with
// kReplaceSyncPromosWithSignInPromos, the user is not warned that the browser is managed.
- (void)testSyncTypesDisabledPolicy {
  // Set policy.
  base::Value::List list;
  list.Append("tabs");
  policy_test_utils::SetPolicy(base::Value(std::move(list)),
                               policy::key::kSyncTypesListDisabled);

  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:NO];
  [ChromeEarlGreyUI waitForAppToIdle];

  NSString* policyText =
      GetNSString(IDS_IOS_ENTERPRISE_MANAGED_SIGNIN_LEARN_MORE);
  policyText = [policyText stringByReplacingOccurrencesOfString:@"BEGIN_LINK"
                                                     withString:@""];
  policyText = [policyText stringByReplacingOccurrencesOfString:@"END_LINK"
                                                     withString:@""];

  // Check that the policy warning is presented.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(policyText),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Check that the "learn more link" works.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(kLearnMoreLabel),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitLink),
                                   nil)] performAction:grey_tap()];

  // Check that the policy management page was opened.
  [ChromeEarlGrey waitForWebStateContainingText:kManagedText];
}

// Tests that the sign-in promo disappear when sync is disabled and reappears
// when sync is enabled again.
// Related to crbug.com/1287465.
- (void)testTurnOffSyncDisablePolicy {
  // Disable sync by policy.
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(GetNSString(
                                       IDS_IOS_SYNC_SYNC_DISABLED_CONTINUE)),
                                   grey_userInteractionEnabled(), nil)]
      performAction:grey_tap()];
  // Open other device tab.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Check that the sign-in promo is not visible.
  [SigninEarlGreyUI verifySigninPromoNotVisible];
  // Add an identity to generate a SSO identity update notification.
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Enable sync.
  policy_test_utils::SetPolicy(false, policy::key::kSyncDisabled);
  [ChromeEarlGreyUI waitForAppToIdle];
  // Check that the sign-in promo is visible.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollToContentEdgeWithStartPoint(
                               kGREYContentEdgeBottom, 0.5, 0.5)
      onElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kRecentTabsTableViewControllerAccessibilityIdentifier),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests to dismiss the sign-in view by swipe down without an identity.
// See http://crbug.com/1434238.
// kReplaceSyncPromosWithSignInPromos is disabled because, when enabled,
// the add account view is directly opened, skipping the sign-in view.
- (void)testSwipeDownSignInViewWithoutAnIdentity {
  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:NO];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kUnifiedConsentScrollViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  // Test no crash.
}

// Tests in the sign-in+sync consent dialog, that the primary button title is
// "Add Account" when there is no account, and the title is updated to
// "Yes, I'm In" once the account has been added.
// This test can be removed once `ReplaceSyncPromosWithSignInPromos` flag is
// removed.
// Related to crbug.com/1497272.
- (void)
    testPrimaryAccountLabelUpdate_ReplaceSyncPromosWithSignInPromosDisabled {
  // "Add Account" button matcher (with title and accessibility identifier).
  id<GREYMatcher> addAccountButtonMatcher = grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_ADD_ACCOUNT)),
      grey_accessibilityID(kAddAccountAccessibilityIdentifier), nil);
  // "Yes, I'm In" button matcher (with title and accessibility identifier).
  id<GREYMatcher> yesIamInButtonMatcher = grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON)),
      grey_accessibilityID(kConfirmationAccessibilityIdentifier), nil);
  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:NO];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Verify that the "Add Account" button is visible and the "Yes, I'm In" is
  // not.
  [[EarlGrey selectElementWithMatcher:addAccountButtonMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:yesIamInButtonMatcher]
      assertWithMatcher:grey_notVisible()];
  // Set up a fake identity to add and sign-in with.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentityForSSOAuthAddAccountFlow:fakeIdentity];
  // Open Add Account screen.
  [SigninEarlGreyUI tapAddAccountButton];
  [ChromeEarlGreyUI waitForAppToIdle];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(
                                       kFakeAuthAddAccountButtonIdentifier),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Verify that the "Yes, I'm In" button is visible and the "Add Account" is
  // not.
  [[EarlGrey selectElementWithMatcher:yesIamInButtonMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:addAccountButtonMatcher]
      assertWithMatcher:grey_notVisible()];
}

@end
