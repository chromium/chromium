// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/test/separate_profiles_util.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/authentication/account_menu/public/account_menu_constants.h"
#import "ios/chrome/browser/authentication/history_sync/public/pref_names.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

id<GREYMatcher> IdentityDiscMatcher() {
  return grey_accessibilityID(kNTPFeedHeaderIdentityDisc);
}

id<GREYMatcher> AccountMenuMatcher() {
  return grey_accessibilityID(kAccountMenuTableViewId);
}

void TapIdentityDisc() {
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:IdentityDiscMatcher()];
  [[EarlGrey selectElementWithMatcher:IdentityDiscMatcher()]
      performAction:grey_tap()];
}

void OpenAccountMenu() {
  TapIdentityDisc();
  // Ensure the Account Menu is displayed.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:AccountMenuMatcher()];
}

void OpenManageAccountsView() {
  OpenAccountMenu();
  // Tap on the Ellipsis button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kAccountMenuSecondaryActionMenuButtonId)]
      performAction:grey_tap()];
  // Tap on Manage accounts.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_text(l10n_util::GetNSString(
                                       IDS_IOS_ACCOUNT_MENU_EDIT_ACCOUNT_LIST)),
                                   grey_interactable(), nil)]
      performAction:grey_tap()];
  // Checks the manage accounts view is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsEditAccountListTableViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

void SignoutFromAccountMenu() {
  OpenAccountMenu();
  // Tap in Sign out.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuSignoutButtonId)]
      performAction:grey_tap()];

  // Dismiss the signout snackbar.
  [SigninEarlGreyUI dismissSignoutSnackbar];
}

void WaitForEnterpriseOnboardingScreen() {
  // Wait for the enterprise onboarding screen for `kWaitForActionTimeout`
  // seconds.
  ConditionBlock enterpriseOnboardingCondition = ^{
    NSError* error;
    [[EarlGrey selectElementWithMatcher:ManagedProfileCreationScreenMatcher()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];

    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForActionTimeout,
                 enterpriseOnboardingCondition),
             @"Enterprise onboarding didn't appear.");
}

id<GREYMatcher> SigninScreenMatcher() {
  return grey_accessibilityID(
      first_run::kFirstRunSignInScreenAccessibilityIdentifier);
}

id<GREYMatcher> ManagedProfileCreationScreenMatcher() {
  return grey_accessibilityID(
      kManagedProfileCreationScreenAccessibilityIdentifier);
}

id<GREYMatcher> BrowsingDataManagementScreenMatcher() {
  return grey_accessibilityID(
      kBrowsingDataManagementScreenAccessibilityIdentifier);
}

id<GREYMatcher> HistoryScreenMatcher() {
  return grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier);
}

id<GREYMatcher> DefaultBrowserScreenMatcher() {
  return grey_accessibilityID(
      first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier);
}

id<GREYMatcher> AccountMenuSecondaryAccountsButtonMatcher() {
  return grey_accessibilityID(kAccountMenuSecondaryAccountButtonId);
}

id<GREYMatcher> ContinueButtonWithIdentityMatcher(
    FakeSystemIdentity* fakeIdentity) {
  NSString* buttonTitle = l10n_util::GetNSStringF(
      IDS_IOS_FIRST_RUN_SIGNIN_CONTINUE_AS,
      base::SysNSStringToUTF16(fakeIdentity.userGivenName));
  id<GREYMatcher> matcher =
      grey_allOf(grey_accessibilityLabel(buttonTitle),
                 grey_accessibilityTrait(UIAccessibilityTraitStaticText),
                 grey_sufficientlyVisible(), nil);

  return matcher;
}

void ClearHistorySyncPrefs() {
  [ChromeEarlGrey clearUserPrefWithName:history_sync_prefs::
                                            kHistorySyncSuccessiveDeclineCount];
  [ChromeEarlGrey clearUserPrefWithName:history_sync_prefs::
                                            kHistorySyncLastDeclinedTimestamp];
}
