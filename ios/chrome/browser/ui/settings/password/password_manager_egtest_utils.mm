// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_manager_egtest_utils.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_constants.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ButtonWithAccessibilityLabel;

namespace {

// Returns the expected text of the Password Manager's Password Checkup cell for
// the given `state`.
NSString* GetTextForPasswordCheckUIState(PasswordCheckUIState state) {
  if (state == PasswordCheckStateRunning) {
    return l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ONGOING);
  } else {
    return l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP);
  }
}

// Returns the expected detail text of the Password Manager's Password Checkup
// cell for the given `state`. `number` is the number that is expected to be in
// the detail text. Depending on the `state`, it can be the number of sites and
// apps for which there are saved passwords or the number of insecure passwords.
NSString* GetDetailTextForPasswordCheckUIState(PasswordCheckUIState state,
                                               int number) {
  switch (state) {
    case PasswordCheckStateSafe:
      return [NSString
          stringWithFormat:
              @"%@. %@", @"Checked just now",
              l10n_util::GetNSString(
                  IDS_IOS_PASSWORD_CHECKUP_SAFE_STATE_ACCESSIBILITY_LABEL)];

    case PasswordCheckStateUnmutedCompromisedPasswords:
      return base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_PASSWORD_CHECKUP_COMPROMISED_COUNT, number));
    case PasswordCheckStateReusedPasswords:
      return l10n_util::GetNSStringF(IDS_IOS_PASSWORD_CHECKUP_REUSED_COUNT,
                                     base::NumberToString16(number));
    case PasswordCheckStateWeakPasswords:
      return base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_PASSWORD_CHECKUP_WEAK_COUNT, number));
    case PasswordCheckStateDismissedWarnings:
      return base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_PASSWORD_CHECKUP_DISMISSED_COUNT, number));
    case PasswordCheckStateDisabled:
    case PasswordCheckStateDefault:
      return l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_DESCRIPTION);
    case PasswordCheckStateRunning:
      return base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_PASSWORD_CHECKUP_SITES_AND_APPS_COUNT, number));
    case PasswordCheckStateError:
    case PasswordCheckStateSignedOut:
      return l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ERROR);
  }
}

id<GREYMatcher> DeletePasswordConfirmationButton() {
  return grey_allOf(ButtonWithAccessibilityLabel(
                        l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE)),
                    grey_interactable(), nullptr);
}

}  // anonymous namespace

namespace password_manager_test_utils {

#pragma mark - Matchers

id<GREYMatcher> PasswordManagerEmptyView() {
  return grey_text(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_EMPTY_TITLE));
}

id<GREYMatcher> PasswordCheckupCellForState(PasswordCheckUIState state,
                                            int number) {
  NSString* text = GetTextForPasswordCheckUIState(state);
  NSString* detail_text = GetDetailTextForPasswordCheckUIState(state, number);
  return grey_accessibilityLabel(
      [NSString stringWithFormat:@"%@, %@", text, detail_text]);
}

id<GREYMatcher> PasswordIssuesTableView() {
  return grey_accessibilityID(kPasswordIssuesTableViewID);
}

id<GREYMatcher> PasswordDetailPassword() {
  return chrome_test_util::TextFieldForCellWithLabelId(
      IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);
}

id<GREYMatcher> NavigationBarEditButton() {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                        IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
                    grey_not(chrome_test_util::TabGridEditButton()),
                    grey_userInteractionEnabled(), nil);
}

id<GREYMatcher> EditDoneButton() {
  return grey_accessibilityID(kSettingsToolbarEditDoneButtonId);
}

id<GREYMatcher> EditPasswordConfirmationButton() {
  return grey_allOf(ButtonWithAccessibilityLabel(
                        l10n_util::GetNSString(IDS_IOS_CONFIRM_PASSWORD_EDIT)),
                    grey_interactable(), nullptr);
}

id<GREYMatcher> UsernameTextfieldForUsernameAndSites(NSString* username,
                                                     NSString* sites) {
  return grey_allOf(
      grey_accessibilityID([NSString
          stringWithFormat:@"%@%@%@", kUsernameTextfieldForPasswordDetailsID,
                           username, sites]),
      grey_interactable(), nullptr);
}

id<GREYMatcher> PasswordTextfieldForUsernameAndSites(NSString* username,
                                                     NSString* sites) {
  return grey_allOf(
      grey_accessibilityID([NSString
          stringWithFormat:@"%@%@%@", kPasswordTextfieldForPasswordDetailsID,
                           username, sites]),
      grey_interactable(), nullptr);
}

id<GREYMatcher> DeleteButtonForUsernameAndSites(NSString* username,
                                                NSString* sites) {
  return grey_allOf(
      grey_accessibilityID([NSString
          stringWithFormat:@"%@%@%@", kDeleteButtonForPasswordDetailsID,
                           username, sites]),
      grey_interactable(), nullptr);
}

id<GREYMatcher> ReauthenticationController() {
  return grey_accessibilityID(
      password_manager::kReauthenticationViewControllerAccessibilityIdentifier);
}

id<GREYMatcher> PasswordSettingsTableView() {
  return grey_accessibilityID(kPasswordsSettingsTableViewId);
}

id<GREYMatcher> PasswordDetailsTableViewMatcher() {
  return grey_accessibilityID(kPasswordDetailsTableViewID);
}

id<GREYMatcher> PasswordDetailsShareButtonMatcher() {
  return grey_accessibilityID(kPasswordShareButtonID);
}

GREYElementInteraction* GetInteractionForIssuesListItem(
    id<GREYMatcher> matcher,
    GREYDirection direction) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(direction, kScrollAmount)
      onElementWithMatcher:PasswordIssuesTableView()];
}

GREYElementInteraction* GetInteractionForPasswordIssueEntry(
    NSString* domain,
    NSString* username,
    NSString* compromised_description) {
  NSString* accessibility_label;
  if (compromised_description) {
    accessibility_label =
        [NSString stringWithFormat:@"%@, %@, %@", domain, username,
                                   compromised_description];
  } else {
    accessibility_label =
        [NSString stringWithFormat:@"%@, %@", domain, username];
  }
  return GetInteractionForIssuesListItem(
      ButtonWithAccessibilityLabel(accessibility_label), kGREYDirectionDown);
}

#pragma mark - Saving passwords

void SavePasswordFormToProfileStore(NSString* password,
                                    NSString* username,
                                    NSString* origin) {
  GREYAssert(
      [PasswordSettingsAppInterface saveExamplePasswordToProfileStore:password
                                                             username:username
                                                               origin:origin],
      kPasswordStoreErrorMessage);
}

void SavePasswordFormToAccountStore(NSString* password,
                                    NSString* username,
                                    NSString* origin) {
  GREYAssert(
      [PasswordSettingsAppInterface saveExamplePasswordToAccountStore:password
                                                             username:username
                                                               origin:origin],
      kPasswordStoreErrorMessage);
}

void SaveCompromisedPasswordFormToProfileStore(NSString* password,
                                               NSString* username,
                                               NSString* origin) {
  GREYAssert([PasswordSettingsAppInterface
                 saveCompromisedPasswordToProfileStore:password
                                              username:username
                                                origin:origin],
             kPasswordStoreErrorMessage);
}

void SaveMutedCompromisedPasswordFormToProfileStore(NSString* origin,
                                                    NSString* username,
                                                    NSString* password) {
  GREYAssert([PasswordSettingsAppInterface
                 saveMutedCompromisedPasswordToProfileStore:password
                                                   username:username
                                                     origin:origin],
             kPasswordStoreErrorMessage);
}

#pragma mark - Saving passkeys

void SaveExamplePasskeyToStore(NSString* rpId,
                               NSString* userId,
                               NSString* username,
                               NSString* userDisplayName) {
  [PasswordSettingsAppInterface saveExamplePasskeyToStore:rpId
                                                   userId:userId
                                                 username:username
                                          userDisplayName:userDisplayName];
}

#pragma mark - Helpers

void OpenPasswordManager() {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsMenuPasswordsButton()];
  // The settings page requested results from PasswordStore. Make sure they
  // have already been delivered by posting a task to PasswordStore's
  // background task runner and wait until it is finished. Because the
  // background task runner is sequenced, this means that previously posted
  // tasks are also finished when this function exits.
  [PasswordSettingsAppInterface passwordProfileStoreResultsCount];
}

void TapNavigationBarEditButton() {
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
}

void DeleteCredential(NSString* username, NSString* sites) {
  [[EarlGrey
      selectElementWithMatcher:DeleteButtonForUsernameAndSites(username, sites)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeletePasswordConfirmationButton()]
      performAction:grey_tap()];
}

}  // namespace password_manager_test_utils
