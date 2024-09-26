// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/block_types.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/policy/policy_constants.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/user_selectable_type.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/elements_constants.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
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

namespace {

// Duplicated from ios/chrome/browser/ui/authentication/authentication_flow.mm,
// which is fine since the enum values should never be renumbered.
enum class SigninAccountType {
  kRegular = 0,
  kManaged = 1,
};

// Label used to find the 'Learn more' link.
NSString* const kLearnMoreLabel = @"Learn More";

NSString* const kPassphrase = @"hello";

// Timeout in seconds to wait for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(5);

// Sets parental control capability for the given identity.
void SetParentalControlsCapabilityForIdentity(
    FakeSystemIdentity* fakeIdentity) {
  // The identity must exist in the test storage to be able to set capabilities
  // through the fake identity service.
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kIsSubjectToParentalControlsCapabilityName) : @YES,
                 }];
}

void ExpectSigninConsentHistogram(SigninAccountType signinAccountType) {
  NSError* error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@"Signin.AccountType.SigninConsent"];
  GREYAssertNil(error, @"Failed to record show count histogram");
  error = [MetricsAppInterface expectCount:1
                                 forBucket:static_cast<int>(signinAccountType)
                              forHistogram:@"Signin.AccountType.SigninConsent"];
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

- (void)setUp {
  [super setUp];
  // Remove closed tab history to make sure the sign-in promo is always visible
  // in recent tabs.
  [ChromeEarlGrey clearBrowsingHistory];
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
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

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  config.features_enabled.push_back(kClearDeviceDataOnSignOutForManagedUsers);

  if ([self isRunningTest:@selector(testOpenManageSyncSettingsFromNTP)] ||
      [self isRunningTest:@selector
            (FLAKY_testAccessiblityStringForSignedInUserWithoutName)]) {
    // Once kIdentityDiscAccountMenu is launched, the ADP will open the account
    // menu instead of settings view. It will be safe to remove this test at
    // that point. The new flow is covered in testViewAccountMenu. Note:
    // testOpenManageSyncSettingsFromNTPWhenSigninIsNotAllowedByPolicy should
    // still work when kIdentityDiscAccountMenu is enabled.
    config.features_disabled.push_back(kIdentityDiscAccountMenu);
  }

  return config;
}

// Tests that opening the sign-in screen from the Settings and signing in works
// correctly when there is already an identity on the device.
- (void)testSignInOneUser {
  // Set up a fake identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Check `fakeIdentity` is signed-in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  ExpectSigninConsentHistogram(SigninAccountType::kRegular);
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
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];

  [SigninEarlGreyUI signOut];

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
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];

  // Sign out from the supervised account.
  [SigninEarlGreyUI signOut];

  // Verify bookmarks are available.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI verifyEmptyBackgroundIsAbsent];
}

// Tests that signing out a supervised user account clears the account data.
- (void)testSignOutForSupervisedUserClearAccountData {
  // Sign in with a fake supervised identity.
  FakeSystemIdentity* fakeSupervisedIdentity =
      [FakeSystemIdentity fakeIdentity1];
  SetParentalControlsCapabilityForIdentity(fakeSupervisedIdentity);
  [SigninEarlGreyUI signinWithFakeIdentity:fakeSupervisedIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kAccount];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Sign out from the supervised account.
  [SigninEarlGreyUI signOut];

  // Verify bookmarks are cleared.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI verifyEmptyBackgroundAppears];
}

// Tests that signing out from the Settings works correctly.
// TODO(crbug.com/40070966): Evaluate if the test is relevant with
- (void)testSignInDisconnectFromChrome {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signOut];
}

// Tests that signing out of a managed account from the Settings works
// correctly.
// TODO(crbug.com/369617405): Disabled due to flakiness.
- (void)DISABLED_testSignInDisconnectFromChromeManaged_ClearDataFeatureDisabled {
  // Sign-in with a managed account.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Check `fakeIdentity` is signed-in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  ExpectSigninConsentHistogram(SigninAccountType::kManaged);

  [SigninEarlGreyUI signOut];
}

// TODO(crbug.com/368595150): Disabled due to flakiness.
- (void)DISABLED_testSignInDisconnectFromChromeManaged_ClearDataFeatureEnabled {
  // Sign-in with a managed account.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Check `fakeIdentity` is signed-in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  ExpectSigninConsentHistogram(SigninAccountType::kManaged);

  [ChromeEarlGreyUI openSettingsMenu];

  // Open the "Account Settings" view.
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsAccountButton()];

  // We're now in the "manage sync" view, and the signout button is at the very
  // bottom. Scroll there.
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:scrollViewMatcher]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "Sign out" button.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_ITEM))]
      performAction:grey_tap()];

  // Click on signout in the dialog.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::AlertAction(l10n_util::GetNSString(
                         IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON)),
                     grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Close the snackbar, so that it can't obstruct other UI items.
  [SigninEarlGreyUI dismissSignoutSnackbar];

  // Wait until the user is signed out. Use a longer timeout for cases where
  // sign out also triggers a clear browsing data.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:SettingsDoneButton()
                                  timeout:base::test::ios::
                                              kWaitForClearBrowsingDataTimeout];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];
}

// Opens the sign in screen and then cancel it by opening a new tab. Ensures
// that the sign in screen is correctly dismissed. crbug.com/462200
- (void)testSignInCancelIdentityPicker {
  // Add an identity to avoid arriving on the Add Account screen when opening
  // sign-in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self openSigninFromView:OpenSigninMethodFromSettings];
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
  [self openSigninFromView:OpenSigninMethodFromSettings];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity.userEmail)]
      performAction:grey_tap()];

  // Verifies that the Chrome sign-in view is visible.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kIdentityButtonControlIdentifier),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Close sign-in screen and Settings.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(ButtonWithAccessibilityLabelId(
                                IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON),
                            grey_userInteractionEnabled(), nil)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

#pragma mark - Dismiss tests

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: setting menu.
// Interrupted at: user consent.
- (void)testDismissSigninFromSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromSettings];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: recent tabs.
// Interrupted at: user consent.
- (void)testDismissSigninFromRecentTabs {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromRecentTabs];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: tab switcher.
// Interrupted at: user consent.
- (void)testDismissSigninFromTabSwitcher {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the sign-in flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when "
                           @"Tab Group Sync is enabled.");
  }

  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromTabSwitcher];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: tab switcher.
// Interrupted at: identity picker.
- (void)testDismissSigninFromTabSwitcherFromIdentityPicker {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the sign-in flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when "
                           @"Tab Group Sync is enabled.");
  }

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [self openSigninFromView:OpenSigninMethodFromTabSwitcher];
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
  [SigninEarlGrey triggerReauthDialogWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Open the URL as if it was opened from another app.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];
}

#pragma mark - Utils

// Opens sign-in view.
// `openSigninMethod` is the way to start the sign-in.
- (void)openSigninFromView:(OpenSigninMethod)openSigninMethod {
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
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Starts the sign-in workflow, and simulates opening an URL from another app.
// `openSigninMethod` is the way to start the sign-in.
- (void)assertOpenURLWhenSigninFromView:(OpenSigninMethod)openSigninMethod {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [self openSigninFromView:openSigninMethod];
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
  [self openSigninFromView:OpenSigninMethodFromSettings];
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
  [self openSigninFromView:OpenSigninMethodFromSettings];
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
- (void)testSignInCancelAddAccount {
  // Add an identity to avoid arriving on the Add Account screen when opening
  // sign-in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self openSigninFromView:OpenSigninMethodFromSettings];
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
  [self openSigninFromView:OpenSigninMethodFromSettings];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity.userEmail)]
      performAction:grey_tap()];

  // Verifies that the Chrome sign-in view is visible.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kIdentityButtonControlIdentifier),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Close sign-in screen and Settings.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(ButtonWithAccessibilityLabelId(
                                IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON),
                            grey_userInteractionEnabled(), nil)]
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

  [self openSigninFromView:OpenSigninMethodFromSettings];
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
// TODO(crbug.com/331928746): Test flaky.
- (void)FLAKY_testAccessiblityStringForSignedInUserWithoutName {
  // Sign in to Chrome.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
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
      [SigninEarlGrey
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be disabled.");
  GREYAssertFalse(
      [SigninEarlGrey isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
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
      [SigninEarlGrey
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be enabled.");
  GREYAssertTrue(
      [SigninEarlGrey isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
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
- (void)testSyncOnWhenPassphraseIntroducedAfterSignIn {
  [ChromeEarlGrey addSyncPassphrase:kPassphrase];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self openSigninFromView:OpenSigninMethodFromSettings];
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kWebSigninPrimaryButtonAccessibilityIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::SigninScreenPromoSecondaryButtonMatcher()]
      performAction:grey_tap()];

  // Give the Sync state a chance to finish UI updates.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      GoogleServicesSettingsButton()];

  [[EarlGrey selectElementWithMatcher:SettingsAccountButton()]
      performAction:grey_tap()];

  // Checks the user is invited to enter the passphrase.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSyncErrorButtonIdentifier)]
      assertWithMatcher:grey_notNil()];

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

  [[EarlGrey selectElementWithMatcher:SettingsAccountButton()]
      performAction:grey_tap()];

  // Check the user is not invited to enter the passphrase
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSyncErrorButtonIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that the sign-in promo disappear when sync is disabled and reappears
// when sync is enabled again.
// Related to crbug.com/1287465.
- (void)testTurnOffSyncDisablePolicy {
  // When Tab Groups is the third panel (i.e. when Tab Group Sync is enabled),
  // Recent Tabs is not reachable from the Tab Grid. So the sign-in flow is not
  // supported with Tab Group Sync enabled.
  if ([ChromeEarlGrey isTabGroupSyncEnabled]) {
    EARL_GREY_TEST_SKIPPED(@"Recent Tabs is not available in Tab Grid when "
                           @"Tab Group Sync is enabled.");
  }

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

@end
