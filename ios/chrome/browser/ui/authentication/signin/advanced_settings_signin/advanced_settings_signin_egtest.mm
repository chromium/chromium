// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "GREYMatchersShorthand.h"
#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "build/branding_buildflags.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/bookmarks/common/storage_type.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/ntp/home/features.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using chrome_test_util::AdvancedSyncSettingsDoneButtonMatcher;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::GoogleSyncSettingsButton;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsLink;
using chrome_test_util::SettingsSignInRowMatcher;
using l10n_util::GetNSString;
using testing::ButtonWithAccessibilityLabel;

namespace {

NSString* const kPassphrase = @"hello";

// Timeout in seconds to wait for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(5);

// Waits for the settings done button to be enabled.
void WaitForSettingDoneButton() {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForClearBrowsingDataTimeout, condition),
             @"Settings done button not visible");
}

}  // namespace

// Interaction tests for advanced settings sign-in.
@interface AdvancedSettingsSigninTestCase : WebHttpServerChromeTestCase
@end

@implementation AdvancedSettingsSigninTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // When kReplaceSyncPromosWithSignInPromos is enabled, the advanced sync
  // setup view doesn't exist.
  // All tests in AdvancedSettingsSigninTestCase are for this view.
  config.features_disabled.push_back(
      syncer::kReplaceSyncPromosWithSignInPromos);

  if ([self isRunningTest:@selector
            (testInterruptAdvancedSigninBookmarksFromAdvancedSigninSettings)]) {
    // TODO(crbug.com/1455018): Re-enable the flag for non-legacy tests.
    config.features_disabled.push_back(syncer::kEnableBookmarksAccountStorage);
  }
  return config;
}

- (void)setUp {
  [super setUp];

  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [BookmarkEarlGrey clearBookmarks];
}

- (void)tearDown {
  [super tearDown];
  [BookmarkEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];
}

// Tests that signing in, tapping the Settings link on the confirmation screen
// and closing the advanced sign-in settings correctly leaves the user signed
// in.
- (void)testSignInOpenSyncSettings {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsSignInRowMatcher()];
  [[EarlGrey selectElementWithMatcher:SettingsLink()] performAction:grey_tap()];

  // This wait is required because, on devices, EG-test may tap on the button
  // while it is sliding up, which cause the tap to misses the button.
  [ChromeEarlGreyUI waitForAppToIdle];
  [[EarlGrey selectElementWithMatcher:AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];
  [SigninEarlGreyUI tapSigninConfirmationDialog];
  // Test the user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests the following scenario:
//  * Open the advanced link
//  * Open the encryption view
//  * Open the create syncpassphrase view
//  * Interrupt the sign-in by opening a link
// Result: the link should be opened without a crash.
// See http://crbug.com/1424870.
- (void)testInterruptSyncPassphraseEdition {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Open the sync passphrase editor through the sign-in flow.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsSignInRowMatcher()];
  [[[EarlGrey selectElementWithMatcher:SettingsLink()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kUnifiedConsentScrollViewIdentifier)]
      performAction:grey_tap()];
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                           kEncryptionAccessibilityIdentifier)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kManageSyncTableViewAccessibilityIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_SYNC_FULL_ENCRYPTION_DATA))]
      performAction:grey_tap()];
  // Interrupt the flow by opening an URL.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  // Result: the URL is opened without a crash.
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];
}

// Tests that a user that signs in and gives sync consent can sign
// out through the "Sign out and Turn Off Sync" > "Clear Data" option in Sync
// settings.
- (void)testSignInOpenSyncSettingsSignOutAndTurnOffSyncWithClearData {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsSignInRowMatcher()];
  [[EarlGrey selectElementWithMatcher:SettingsLink()] performAction:grey_tap()];

  // This wait is required because, on devices, EG-test may tap on the button
  // while it is sliding up, which cause the tap to misses the button.
  [ChromeEarlGreyUI waitForAppToIdle];
  [[EarlGrey selectElementWithMatcher:AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];
  [SigninEarlGreyUI tapSigninConfirmationDialog];
  // Test the user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];

  // Sign out and clear data from Sync settings.
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleSyncSettingsButton()];
  [[[EarlGrey selectElementWithMatcher:
                  grey_accessibilityLabel(l10n_util::GetNSString(
                      IDS_IOS_OPTIONS_ACCOUNTS_SIGN_OUT_TURN_OFF_SYNC))]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kManageSyncTableViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON)]
      performAction:grey_tap()];
  WaitForSettingDoneButton();

  // Verify signed out.
  [SigninEarlGrey verifySignedOut];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Verify bookmarks are cleared.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI verifyEmptyBackgroundAppears];
}

// Tests that a user that signs in and gives sync consent can sign
// out through the "Sign out and Turn Off Sync" > "Keep Data" option in Sync
// setting.
- (void)testSignInOpenSyncSettingsSignOutAndTurnOffSyncWithKeepData {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsSignInRowMatcher()];
  [[EarlGrey selectElementWithMatcher:SettingsLink()] performAction:grey_tap()];

  // This wait is required because, on devices, EG-test may tap on the button
  // while it is sliding up, which cause the tap to misses the button.
  [ChromeEarlGreyUI waitForAppToIdle];
  [[EarlGrey selectElementWithMatcher:AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];
  [SigninEarlGreyUI tapSigninConfirmationDialog];
  // Test the user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];

  // Sign out and keep data from Sync settings.
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleSyncSettingsButton()];
  [[[EarlGrey selectElementWithMatcher:
                  grey_accessibilityLabel(l10n_util::GetNSString(
                      IDS_IOS_OPTIONS_ACCOUNTS_SIGN_OUT_TURN_OFF_SYNC))]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kManageSyncTableViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SIGNOUT_DIALOG_KEEP_DATA_BUTTON)]
      performAction:grey_tap()];
  WaitForSettingDoneButton();

  // Verify signed out.
  [SigninEarlGrey verifySignedOut];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Verify bookmarks are available.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI verifyEmptyBackgroundIsAbsent];
}

// Tests that "Sign out and Turn Off Sync" is not present in advanced settings.
- (void)testSignInOpenSyncSettingsNoSignOut {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsSignInRowMatcher()];
  [[EarlGrey selectElementWithMatcher:SettingsLink()] performAction:grey_tap()];

  [[[EarlGrey selectElementWithMatcher:
                  grey_accessibilityLabel(l10n_util::GetNSString(
                      IDS_IOS_OPTIONS_ACCOUNTS_SIGN_OUT_TURN_OFF_SYNC))]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that a user account with a sync password displays a sync error
// message after sign-in.
- (void)testSigninOpenSyncSettingsWithPasswordError {
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsSignInRowMatcher()];
  [[EarlGrey selectElementWithMatcher:SettingsLink()] performAction:grey_tap()];
  // This wait is required because, on devices, EG-test may tap on the button
  // while it is sliding up, which cause the tap to misses the button.
  [ChromeEarlGreyUI waitForAppToIdle];
  [[EarlGrey selectElementWithMatcher:AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];
  [SigninEarlGreyUI tapSigninConfirmationDialog];
  // Test the user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Give the Sync state a chance to finish UI updates.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:GoogleSyncSettingsButton()];
  [[EarlGrey selectElementWithMatcher:GoogleSyncSettingsButton()]
      performAction:grey_tap()];

  // Test the sync error message is visible.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_accessibilityLabel(
                                         l10n_util::GetNSString(
                                             IDS_IOS_SYNC_ERROR_TITLE)),
                                     grey_not(grey_userInteractionEnabled()),
                                     nil)]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition),
             @"Could not find the Sync Error text");
}

// Tests that no sync error will be displayed after a user introduces the sync
// passphrase correctly from Advanced Settings and then signs in.
- (void)testSigninWithPassword {
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsSignInRowMatcher()];
  [[EarlGrey selectElementWithMatcher:SettingsLink()] performAction:grey_tap()];
  // Scroll and select the Encryption item.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(ButtonWithAccessibilityLabelId(
                                              IDS_IOS_MANAGE_SYNC_ENCRYPTION),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kManageSyncTableViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Type and submit the sync passphrase.
  [SigninEarlGreyUI submitSyncPassphrase:kPassphrase];

  [[EarlGrey selectElementWithMatcher:AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];
  [SigninEarlGreyUI tapSigninConfirmationDialog];
  // Check Sync On label is visible and user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySyncUIEnabled:YES];
}

// Tests interrupting sign-in by opening an URL from another app.
// Sign-in opened from: setting menu.
// Interrupted at: advanced sign-in.
- (void)testInterruptAdvancedSigninSettingsFromAdvancedSigninSettings {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsSignInRowMatcher()];
  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey selectElementWithMatcher:SettingsLink()] performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];

  [SigninEarlGrey verifySignedOut];
}

// Tests interrupting sign-in by opening an URL from another app.
// Sign-in opened from: bookmark view.
// Interrupted at: advanced sign-in.
- (void)testInterruptAdvancedSigninBookmarksFromAdvancedSigninSettings {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::BookmarksDestinationButton()];
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsLink()] performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];

  [SigninEarlGrey verifySignedOut];
}

// Tests interrupting sign-in by opening an URL from another app.
// Sign-in opened from: recent tabs.
// Interrupted at: advanced sign-in.
- (void)testInterruptSigninFromRecentTabsFromAdvancedSigninSettings {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI tapPrimarySignInButtonInRecentTabs];
  [[EarlGrey selectElementWithMatcher:SettingsLink()] performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];

  [SigninEarlGrey verifySignedOut];
}

// Tests interrupting sign-in by opening an URL from another app.
// Sign-in opened from: tab switcher.
// Interrupted at: advanced sign-in.
- (void)testInterruptSigninFromTabSwitcherFromAdvancedSigninSettings {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI tapPrimarySignInButtonInTabSwitcher];
  [[EarlGrey selectElementWithMatcher:SettingsLink()] performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];

  [SigninEarlGrey verifySignedOut];
}

// Tests that canceling sign-in from advanced sign-in settings will
// return the user to their prior sign-in state.
- (void)testCancelSigninFromAdvancedSigninSettings {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsSignInRowMatcher()];
  [[EarlGrey selectElementWithMatcher:SettingsLink()] performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey selectElementWithMatcher:AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedOut];
}

@end
