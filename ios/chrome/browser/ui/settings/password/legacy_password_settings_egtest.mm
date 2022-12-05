// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <TargetConditionals.h>

#import <utility>

#import "base/callback.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/metrics/metrics_app_interface.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_constants.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "ui/base/l10n/l10n_util.h"

#import "ios/third_party/earl_grey2/src/CommonLib/Matcher/GREYLayoutConstraint.h"  // nogncheck

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// This class integration tests the existing behavior of password settings,
// prior to the introduction of the `kIOSPasswordUISplit` flag.

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::TabGridEditButton;
using chrome_test_util::TextFieldForCellWithLabelId;
using chrome_test_util::TurnTableViewSwitchOn;

namespace {

// How many points to scroll at a time when searching for an element. Setting it
// too low means searching takes too long and the test might time out. Setting
// it too high could result in scrolling way past the searched element.
constexpr int kScrollAmount = 150;

NSString* GetTextFieldForID(int category_id) {
  return [NSString
      stringWithFormat:@"%@_textField", l10n_util::GetNSString(category_id)];
}

// Returns the GREYElementInteraction* for the item on the password list with
// the given `matcher`. It scrolls in `direction` if necessary to ensure that
// the matched item is sufficiently visible, thus interactable.
GREYElementInteraction* GetInteractionForListItem(id<GREYMatcher> matcher,
                                                  GREYDirection direction) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_sufficientlyVisible(),
                                          nil)]
         usingSearchAction:grey_scrollInDirection(direction, kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)];
}

// Returns the GREYElementInteraction* for the item on the password issues list
// with the given `matcher`. It scrolls in `direction` if necessary to ensure
// that the matched item is interactable.
GREYElementInteraction* GetInteractionForIssuesListItem(
    id<GREYMatcher> matcher,
    GREYDirection direction) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(direction, kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(kPasswordIssuesTableViewId)];
}

// Returns the GREYElementInteraction* for the cell on the password list with
// the given `username`. It scrolls down if necessary to ensure that the matched
// cell is interactable.
GREYElementInteraction* GetInteractionForPasswordEntry(NSString* username) {
  return GetInteractionForListItem(ButtonWithAccessibilityLabel(username),
                                   kGREYDirectionDown);
}

// Returns the GREYElementInteraction* for the cell on the password list with
// the given `username`. It scrolls down if necessary to ensure that the matched
// cell is interactable.
GREYElementInteraction* GetInteractionForPasswordIssueEntry(
    NSString* username) {
  return GetInteractionForIssuesListItem(ButtonWithAccessibilityLabel(username),
                                         kGREYDirectionDown);
}

// Returns the GREYElementInteraction* for the item on the detail view
// identified with the given `matcher`. It scrolls down if necessary to ensure
// that the matched cell is interactable.
GREYElementInteraction* GetInteractionForPasswordDetailItem(
    id<GREYMatcher> matcher) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_interactable(), nil)]
         usingSearchAction:grey_scrollToContentEdge(kGREYContentEdgeTop)
      onElementWithMatcher:grey_accessibilityID(kPasswordDetailsTableViewId)];
}

// Returns the GREYElementInteraction* for the item on the deletion alert
// identified with the given `matcher`.
GREYElementInteraction* GetInteractionForPasswordsExportConfirmAlert(
    id<GREYMatcher> matcher) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_interactable(), nil)]
      inRoot:grey_accessibilityID(kPasswordsExportConfirmViewId)];
}

GREYElementInteraction* GetPasswordDetailTextFieldWithID(int detail_id) {
  return GetInteractionForPasswordDetailItem(
      grey_allOf(grey_accessibilityID(GetTextFieldForID(detail_id)),
                 grey_kindOfClassName(@"UITextField"), nil));
}

// Matcher for "Saved Passwords" header in the password list.
id<GREYMatcher> SavedPasswordsHeaderMatcher() {
  return grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_SAVED_HEADING)),
      grey_accessibilityTrait(UIAccessibilityTraitHeader), nullptr);
}

// Matcher for a UITextField inside a SettingsSearchCell.
id<GREYMatcher> SearchTextField() {
  return grey_accessibilityID(kPasswordsSearchBarId);
}

GREYLayoutConstraint* Below() {
  return [GREYLayoutConstraint
      layoutConstraintWithAttribute:kGREYLayoutAttributeTop
                          relatedBy:kGREYLayoutRelationGreaterThanOrEqual
               toReferenceAttribute:kGREYLayoutAttributeBottom
                         multiplier:1.0
                           constant:0.0];
}

// Matcher for the website in Password Details view.
id<GREYMatcher> PasswordDetailWebsite() {
  return TextFieldForCellWithLabelId(IDS_IOS_SHOW_PASSWORD_VIEW_SITE);
}

// Matcher for the username in Password Details view.
id<GREYMatcher> PasswordDetailUsername() {
  return TextFieldForCellWithLabelId(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
}

// Matcher for the password in Password Details view.
id<GREYMatcher> PasswordDetailPassword() {
  return TextFieldForCellWithLabelId(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);
}

// Matcher for the federation details in Password Details view.
id<GREYMatcher> PasswordDetailFederation() {
  return grey_allOf(grey_accessibilityID(GetTextFieldForID(
                        IDS_IOS_SHOW_PASSWORD_VIEW_FEDERATION)),
                    grey_kindOfClassName(@"UITextField"), nil);
}

// Matcher for the Show password button in Password Details view.
id<GREYMatcher> ShowPasswordButton() {
  return grey_allOf(ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON)),
                    grey_interactable(), nullptr);
}

// Matcher for the Hide password button in Password Details view.
id<GREYMatcher> HidePasswordButton() {
  return grey_allOf(ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON)),
                    grey_interactable(), nullptr);
}

// Matcher for the Delete button in Password Details view.
id<GREYMatcher> DeleteButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_SETTINGS_TOOLBAR_DELETE),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)),
      nullptr);
}

// Matcher for the Delete button in Confirmation Alert for password deletion.
id<GREYMatcher> DeleteConfirmationButton() {
  return grey_allOf(ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_CONFIRM_PASSWORD_DELETION)),
                    grey_interactable(), nullptr);
}

// Matcher for the Delete button in the list view, located at the bottom of the
// screen.
id<GREYMatcher> DeleteButtonAtBottom() {
  return grey_accessibilityID(kSettingsToolbarDeleteButtonId);
}

// Return the edit button from the navigation bar.
id<GREYMatcher> NavigationBarEditButton() {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                        IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
                    grey_not(TabGridEditButton()),
                    grey_userInteractionEnabled(), nil);
}

// Matcher for the Confirm button in Confirmation Alert for password editing.
id<GREYMatcher> EditConfirmationButton() {
  return grey_allOf(ButtonWithAccessibilityLabel(
                        l10n_util::GetNSString(IDS_IOS_CONFIRM_PASSWORD_EDIT)),
                    grey_interactable(), nullptr);
}

// Matcher for the "View Password" Button presented when a duplicated credential
// is found in the add credential flow.
id<GREYMatcher> DuplicateCredentialViewPasswordButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_PASSWORD_SETTINGS_VIEW_PASSWORD_BUTTON)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    nullptr);
}

// Matcher for the "Set up…"
GREYElementInteraction* SetUpTrustedVaultLink() {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kOnDeviceEncryptionSetUpId),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)];
}

// Matcher for the link allowing to learn more about
// on device encryption. Only present when the user has opted-in.
GREYElementInteraction* OptedInTrustedVaultLink() {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kOnDeviceEncryptionLearnMoreId),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)];
}

// Matcher for the link explaining Trusted Vault.
// Only present when the user has opted-in.
GREYElementInteraction* OptedInTrustedVaultText() {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kOnDeviceEncryptionOptedInTextId),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)];
}

// Matcher for the text offering the user to opt-in trusted vault.
GREYElementInteraction* OptInTrustedVaultLink() {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kOnDeviceEncryptionOptInId),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)];
}

// Matches the pop-up (call-out) menu item with accessibility label equal to the
// translated string identified by `label`.
id<GREYMatcher> PopUpMenuItemWithLabel(int label) {
  // iOS13 reworked menu button subviews to no longer be accessibility
  // elements.  Multiple menu button subviews no longer show up as potential
  // matches, which means the matcher logic does not need to be as complex as
  // the iOS 11/12 logic.  Various table view cells may share the same
  // accesibility label, but those can be filtered out by ignoring
  // UIAccessibilityTraitButton.
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(label)),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitButton)),
      grey_userInteractionEnabled(), nil);
}

// Returns matcher for the "Add Password" button located at the bottom of the
// screen.
id<GREYMatcher> AddPasswordButton() {
  return grey_accessibilityID(kSettingsToolbarAddButtonId);
}

// Returns matcher for the "Save" button in the "Add Password" view.
id<GREYMatcher> AddPasswordSaveButton() {
  return grey_accessibilityID(kPasswordsAddPasswordSaveButtonId);
}

// Matcher for the toolbar's edit done button.
id<GREYMatcher> SettingToolbarEditDoneButton() {
  return grey_accessibilityID(kSettingsToolbarEditDoneButtonId);
}

// Saves an example form in the store.
void SaveExamplePasswordForm() {
  GREYAssert(
      [PasswordSettingsAppInterface saveExamplePassword:@"concrete password"
                                               userName:@"concrete username"
                                                 origin:@"https://example.com"],
      @"Stored form was not found in the PasswordStore results.");
}

// Saves two example forms in the store.
void SaveExamplePasswordForms() {
  GREYAssert([PasswordSettingsAppInterface
                 saveExamplePassword:@"password1"
                            userName:@"user1"
                              origin:@"https://example11.com"],
             @"Stored form was not found in the PasswordStore results.");
  GREYAssert([PasswordSettingsAppInterface
                 saveExamplePassword:@"password2"
                            userName:@"user2"
                              origin:@"https://example12.com"],
             @"Stored form was not found in the PasswordStore results.");
}

// Saves two example blocked forms in the store.
void SaveExampleBlockedForms() {
  GREYAssert([PasswordSettingsAppInterface
                 saveExampleBlockedOrigin:@"https://exclude1.com"],
             @"Stored form was not found in the PasswordStore results.");
  GREYAssert([PasswordSettingsAppInterface
                 saveExampleBlockedOrigin:@"https://exclude2.com"],
             @"Stored form was not found in the PasswordStore results.");
}

// Opens the passwords page from the NTP. It requires no menus to be open.
void OpenPasswordSettings() {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsMenuPasswordsButton()];
  // The settings page requested results from PasswordStore. Make sure they
  // have already been delivered by posting a task to PasswordStore's
  // background task runner and waits until it is finished. Because the
  // background task runner is sequenced, this means that previously posted
  // tasks are also finished when this function exits.
  [PasswordSettingsAppInterface passwordStoreResultsCount];
}

// Tap Edit in any settings view.
void TapEdit() {
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
}

void CopyPasswordDetailWithID(int detail_id) {
  [GetPasswordDetailTextFieldWithID(detail_id) performAction:grey_tap()];

  // Tap the context menu item for copying.
  [[EarlGrey selectElementWithMatcher:PopUpMenuItemWithLabel(
                                          IDS_IOS_SETTINGS_SITE_COPY_MENU_ITEM)]
      performAction:grey_tap()];
}

id<GREYMatcher> EditDoneButton() {
  return SettingToolbarEditDoneButton();
}

}  // namespace

// Various tests for the Save Passwords section of the settings.
@interface LegacyPasswordSettingsTestCase : ChromeTestCase
@end

@implementation LegacyPasswordSettingsTestCase

- (void)setUp {
  [super setUp];
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
}

- (void)tearDown {
  // Snackbars triggered by tests stay up for a limited time even if the
  // settings get closed. Ensure that they are closed to avoid interference with
  // other tests.
  [PasswordSettingsAppInterface dismissSnackBar];
  GREYAssert([PasswordSettingsAppInterface clearPasswordStore],
             @"PasswordStore was not cleared.");

  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");

  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;

  config.features_disabled.push_back(
      password_manager::features::kIOSPasswordUISplit);

  if ([self isRunningTest:@selector
            (testNoOndeviceEncryptionSetupWhenSignedOut)]) {
    config.features_enabled.push_back(syncer::kSyncTrustedVaultPassphrasePromo);
  }
  if ([self isRunningTest:@selector(testNoOndeviceEncryptionWithoutFlag)]) {
    config.features_disabled.push_back(
        syncer::kSyncTrustedVaultPassphrasePromo);
  }

  return config;
}

// Verifies that a signed out account has no option related to
// on device encryption.
- (void)testNoOndeviceEncryptionWithoutFlag {
  OpenPasswordSettings();

  // Check that the menus related to on-device encryptions are not displayed.
  [OptedInTrustedVaultLink() assertWithMatcher:grey_nil()];
  [OptedInTrustedVaultText() assertWithMatcher:grey_nil()];
  [OptInTrustedVaultLink() assertWithMatcher:grey_nil()];
  [SetUpTrustedVaultLink() assertWithMatcher:grey_nil()];
}

// Check that a user which is not logged in any account do not get
// offered to use trusted vault.
- (void)testNoOndeviceEncryptionSetupWhenSignedOut {
  OpenPasswordSettings();

  // Check that the menus related to on-device encryptions are not displayed.
  [OptedInTrustedVaultLink() assertWithMatcher:grey_nil()];
  [OptedInTrustedVaultText() assertWithMatcher:grey_nil()];
  [OptInTrustedVaultLink() assertWithMatcher:grey_nil()];
  [SetUpTrustedVaultLink() assertWithMatcher:grey_nil()];
}

// Verifies the UI elements are accessible on the Passwords page.
- (void)testAccessibilityOnPasswords {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  TapEdit();
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      performAction:grey_tap()];

  // Inspect "password details" view.
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to copy a password provide appropriate feedback,
// both when reauthentication succeeds and when it fails.
- (void)testCopyPasswordToast {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Check the snackbar in case of successful reauthentication.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  CopyPasswordDetailWithID(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);

  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  // Check the snackbar in case of failed reauthentication.
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kFailure];

  CopyPasswordDetailWithID(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);

  snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_NOT_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that an attempt to show a password provides an appropriate feedback
// when reauthentication succeeds.
- (void)testShowPasswordAuthSucceeded {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      performAction:grey_tap()];

  // Ensure that password is shown.
  [GetInteractionForPasswordDetailItem(grey_textFieldValue(
      @"concrete password")) assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that an attempt to show a password provides an appropriate feedback
// when reauthentication fails.
- (void)testShowPasswordToastAuthFailed {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kFailure];

  // Check the snackbar in case of failed reauthentication.
  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      performAction:grey_tap()];

  // Check that the password is not displayed.
  [[EarlGrey selectElementWithMatcher:grey_textFieldValue(@"concrete password")]
      assertWithMatcher:grey_nil()];

  // Note that there is supposed to be no message (cf. the case of the copy
  // button, which does need one). The reason is that the password not being
  // shown is enough of a message that the action failed.

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to copy a username provide appropriate feedback.
- (void)testCopyUsernameToast {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  CopyPasswordDetailWithID(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_USERNAME_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to copy a site URL provide appropriate feedback.
- (void)testCopySiteToast {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  CopyPasswordDetailWithID(IDS_IOS_SHOW_PASSWORD_VIEW_SITE);

  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that deleting a saved password from password details view goes back
// to the list-of-passwords view which doesn't display that form anymore.
- (void)testSavedFormDeletionInDetailView {
  // Save form to be deleted later.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteButton()] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteConfirmationButton()]
      performAction:grey_tap()];

  // Wait until the alert and the detail view are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the current view is now the list view, by locating
  // PasswordTableViewController.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)]
      assertWithMatcher:grey_notNil()];

  // Verify that the deletion was propagated to the PasswordStore.
  GREYAssertEqual(0, [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Stored password was not removed from PasswordStore.");

  // Also verify that the removed password is no longer in the list.
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Finally, verify that the Edit button is visible and disabled, because there
  // are no other password entries left for deletion via the "Edit" mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that deleting a duplicated saved password from password details view
// goes back to the list-of-passwords view which doesn't display that form
// anymore.
- (void)testDuplicatedSavedFormDeletionInDetailView {
  // Save form to be deleted later.
  SaveExamplePasswordForm();
  // Save duplicate of the previously saved form to be deleted at the same time.
  // This entry is considered duplicated because it maps to the same sort key
  // as the previous one.
  GREYAssert([PasswordSettingsAppInterface
                 saveExamplePassword:@"concrete password"
                            userName:@"concrete username"
                              origin:@"https://example.com/example"],
             @"Stored form was not found in the PasswordStore results.");

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteButton()] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteConfirmationButton()]
      performAction:grey_tap()];

  // Wait until the alert and the detail view are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the current view is now the list view, by locating
  // PasswordTableViewController.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)]
      assertWithMatcher:grey_notNil()];

  // Verify that the deletion was propagated to the PasswordStore.
  GREYAssertEqual(0, [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Stored password was not removed from PasswordStore.");

  // Also verify that the removed password is no longer in the list.
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Finally, verify that the Edit button is visible and disabled, because there
  // are no other password entries left for deletion via the "Edit" mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that deleting a blocked form from password details view goes
// back to the list-of-passwords view which doesn't display that form anymore.
- (void)testBlockedFormDeletionInDetailView {
  // Save blocked form to be deleted later.
  GREYAssert([PasswordSettingsAppInterface
                 saveExampleBlockedOrigin:@"https://blocked.com"],
             @"Stored form was not found in the PasswordStore results.");

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"blocked.com") performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteButton()] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteConfirmationButton()]
      performAction:grey_tap()];

  // Wait until the alert and the detail view are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the current view is now the list view, by locating
  // PasswordTableViewController.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)]
      assertWithMatcher:grey_notNil()];

  // Verify that the deletion was propagated to the PasswordStore.
  GREYAssertEqual(0, [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Stored password was not removed from PasswordStore.");

  // Also verify that the removed password is no longer in the list.
  [GetInteractionForPasswordEntry(@"secret.com")
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Finally, verify that the Edit button is visible and disabled, because there
  // are no other password entries left for deletion via the "Edit" mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that deleting a password from password details can be cancelled.
- (void)testCancelDeletionInDetailView {
  // Save form to be deleted later.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteButton()] performAction:grey_tap()];

  // Close the dialog by taping on Password Details screen.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsTableViewId)]
      performAction:grey_tap()];

  // Check that the current view is still the detail view.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsTableViewId)]
      assertWithMatcher:grey_notNil()];

  // Verify that the deletion did not happen.
  GREYAssertEqual(1u, [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Stored password was removed from PasswordStore.");

  // Go back to the list view and verify that the password is still in the
  // list.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that if the list view is in edit mode, the "Save Passwords" switch is
// disabled and the details password view is not accessible on tapping the
// entries.
- (void)testEditMode {
  // Save a form to have something to tap on.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  TapEdit();

  // Check that the "Save Passwords" switch is disabled.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSavePasswordSwitchTableViewId, YES,
                                          NO)] assertWithMatcher:grey_notNil()];

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Check that the current view is not the detail view, by failing to locate
  // the Copy button.
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to copy the password via the context menu item provide
// an appropriate feedback.
- (void)testCopyPasswordMenuItem {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Tap the password cell to display the context menu.
  [GetInteractionForPasswordDetailItem(grey_text(kMaskedPassword))
      performAction:grey_tap()];

  // Make sure to capture the reauthentication module in a variable until the
  // end of the test, otherwise it might get deleted too soon and break the
  // functionality of copying and viewing passwords.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  // Tap the context menu item for copying.
  [[EarlGrey
      selectElementWithMatcher:PopUpMenuItemWithLabel(
                                   IDS_IOS_SETTINGS_PASSWORD_COPY_MENU_ITEM)]
      performAction:grey_tap()];

  // Check the snackbar.
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that federated credentials have no password but show the federation.
- (void)testFederated {
  GREYAssert([PasswordSettingsAppInterface
                 saveExampleFederatedOrigin:@"https://famous.provider.net"
                                   userName:@"federated username"
                                     origin:@"https://example.com"],
             @"Stored form was not found in the PasswordStore results.");

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, federated username")
      performAction:grey_tap()];

  // Check that the Site and Username are present and correct.
  [[EarlGrey selectElementWithMatcher:PasswordDetailWebsite()]
      assertWithMatcher:grey_textFieldValue(@"https://example.com/")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_textFieldValue(@"federated username")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailFederation()]
      assertWithMatcher:grey_textFieldValue(@"famous.provider.net")];

  // Check that the password is not present.
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_nil()];

  // Check that editing doesn't require reauth.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kFailure];
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
  // Ensure delete button is present after entering editing mode.
  [[EarlGrey selectElementWithMatcher:DeleteButton()]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks the order of the elements in the detail view layout for a
// non-federated, non-blocked credential.
- (void)testLayoutNormal {
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailWebsite()]
      assertWithMatcher:grey_textFieldValue(@"https://example.com/")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_textFieldValue(@"concrete username")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_textFieldValue(kMaskedPassword)];

  // Check that the federation origin is not present.
  [[EarlGrey selectElementWithMatcher:PasswordDetailFederation()]
      assertWithMatcher:grey_nil()];

  [GetInteractionForPasswordDetailItem(PasswordDetailPassword())
      assertWithMatcher:grey_layout(@[ Below() ], PasswordDetailUsername())];
  [GetInteractionForPasswordDetailItem(PasswordDetailUsername())
      assertWithMatcher:grey_layout(@[ Below() ], PasswordDetailWebsite())];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks the order of the elements in the detail view layout for a blocked
// credential.
- (void)testLayoutForBlockedCredential {
  GREYAssert([PasswordSettingsAppInterface
                 saveExampleBlockedOrigin:@"https://example.com"],
             @"Stored form was not found in the PasswordStore results.");

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com") performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailWebsite()]
      assertWithMatcher:grey_textFieldValue(@"https://example.com/")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailFederation()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks the order of the elements in the detail view layout for a federated
// credential.
- (void)testLayoutFederated {
  GREYAssert([PasswordSettingsAppInterface
                 saveExampleFederatedOrigin:@"https://famous.provider.net"
                                   userName:@"federated username"
                                     origin:@"https://example.com"],
             @"Stored form was not found in the PasswordStore results.");

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, federated username")
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailWebsite()]
      assertWithMatcher:grey_textFieldValue(@"https://example.com/")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_textFieldValue(@"federated username")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailFederation()]
      assertWithMatcher:grey_textFieldValue(@"famous.provider.net")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_nil()];

  [GetInteractionForPasswordDetailItem(PasswordDetailUsername())
      assertWithMatcher:grey_layout(@[ Below() ], PasswordDetailWebsite())];
  [[EarlGrey selectElementWithMatcher:PasswordDetailFederation()]
      assertWithMatcher:grey_layout(@[ Below() ], PasswordDetailUsername())];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Check that stored entries are shown no matter what the preference for saving
// passwords is.
- (void)testStoredEntriesAlwaysShown {
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  // Toggle the "Save Passwords" control off and back on and check that stored
  // items are still present.
  BOOL isSwitchEnabled =
      [PasswordSettingsAppInterface isCredentialsServiceEnabled];
  BOOL kExpectedState[] = {isSwitchEnabled, !isSwitchEnabled};
  for (BOOL expected_state : kExpectedState) {
    // Toggle the switch. It is located near the top, so if not interactable,
    // try scrolling up.
    [GetInteractionForListItem(
        chrome_test_util::TableViewSwitchCell(kSavePasswordSwitchTableViewId,
                                              expected_state),
        kGREYDirectionUp) performAction:TurnTableViewSwitchOn(!expected_state)];

    // Check that the switch has been modified.
    [GetInteractionForListItem(
        chrome_test_util::TableViewSwitchCell(kSavePasswordSwitchTableViewId,
                                              !expected_state),
        kGREYDirectionUp) assertWithMatcher:grey_sufficientlyVisible()];

    // Check the stored items. Scroll down if needed.
    [GetInteractionForPasswordEntry(@"example.com, concrete username")
        assertWithMatcher:grey_notNil()];
  }

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Check that toggling the switch for the "save passwords" preference changes
// the settings.
- (void)testPrefToggle {
  OpenPasswordSettings();

  // Toggle the "Save Passwords" control off and back on and check the
  // preferences.
  constexpr BOOL kExpectedState[] = {YES, NO};
  for (BOOL expected_initial_state : kExpectedState) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                            kSavePasswordSwitchTableViewId,
                                            expected_initial_state)]
        performAction:TurnTableViewSwitchOn(!expected_initial_state)];
    const bool expected_final_state = !expected_initial_state;
    GREYAssertEqual(expected_final_state,
                    [PasswordSettingsAppInterface isCredentialsServiceEnabled],
                    @"State of the UI toggle differs from real preferences.");
  }

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that deleting a password from the list view works.
- (void)testDeletionInListView {
  // Save a password to be deleted later.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  TapEdit();

  // Select password entry to be removed.
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteButtonAtBottom()]
      performAction:grey_tap()];

  // Verify that the deletion was propagated to the PasswordStore.
  GREYAssertEqual(0, [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Stored password was not removed from PasswordStore.");
  // Verify that the removed password is no longer in the list.
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that an attempt to copy a password provides appropriate feedback when
// reauthentication cannot be attempted.
- (void)testCopyPasswordToastNoReauth {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleCanAttempt:NO];

  CopyPasswordDetailWithID(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);

  NSString* title =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(title)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OKButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that an attempt to view a password provides appropriate feedback when
// reauthentication cannot be attempted.
- (void)testShowPasswordToastNoReauth {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleCanAttempt:NO];
  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      performAction:grey_tap()];

  NSString* title =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(title)]
      assertWithMatcher:grey_sufficientlyVisible()];
  NSString* learnHow =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_LEARN_HOW);
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                   learnHow)] performAction:grey_tap()];
  // Check the sub menu is closed due to the help article.
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      assertWithMatcher:grey_notNil()
                  error:&error];
  GREYAssertTrue(error, @"The settings back button is still displayed");

  // Check the settings page is closed.
  error = nil;
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      assertWithMatcher:grey_notNil()
                  error:&error];
  GREYAssertTrue(error, @"The settings page is still displayed");
}

// Test that even with many passwords the settings are still usable. In
// particular, ensure that password entries "below the fold" are reachable and
// their detail view is shown on tapping.
// There are two bottlenecks potentially affecting the runtime of the test:
// (1) Storing passwords on initialisation.
// (2) Speed of EarlGrey UI operations such as scrolling.
// To keep the test duration reasonable, the delay from (1) is eliminated in
// storing just about enough passwords to ensure filling more than one page on
// any device. To limit the effect of (2), custom large scrolling steps are
// added to the usual scrolling actions.
- (void)testManyPasswords {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // TODO(crbug.com/906551): Enable the test on iPad once the bug is fixed.
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad.");
  }

  // Enough just to ensure filling more than one page on all devices.
  constexpr int kPasswordsCount = 15;

  // Send the passwords to the queue to be added to the PasswordStore.
  [PasswordSettingsAppInterface saveExamplePasswordWithCount:kPasswordsCount];

  // Use TestStoreConsumer::GetStoreResults to wait for the background storing
  // task to complete and to verify that the passwords have been stored.
  GREYAssertEqual(kPasswordsCount,
                  [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Unexpected PasswordStore results.");

  OpenPasswordSettings();

  // Wait for the loading indicator to disappear, and the sections to be on
  // screen, before scrolling.
  [[EarlGrey selectElementWithMatcher:SavedPasswordsHeaderMatcher()]
      assertWithMatcher:grey_notNil()];

  // Aim at an entry almost at the end of the list.
  constexpr int kRemoteIndex = kPasswordsCount - 4;
  // The scrolling in GetInteractionForPasswordEntry has too fine steps to
  // reach the desired part of the list quickly. The following gives it a head
  // start of the desired position, counting 30 points per entry and
  // aiming at `kRemoteIndex`.
  constexpr int kJump = kRemoteIndex * 30 + 150;
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)]
      performAction:grey_scrollInDirection(kGREYDirectionDown, kJump)];
  [GetInteractionForPasswordEntry([NSString
      stringWithFormat:@"www%02d.example.com, concrete username %02d",
                       kRemoteIndex, kRemoteIndex]) performAction:grey_tap()];

  // Check that the detail view loaded correctly by verifying the site content.
  [[EarlGrey selectElementWithMatcher:PasswordDetailWebsite()]
      assertWithMatcher:grey_textFieldValue([NSString
                            stringWithFormat:@"https://www%02d.example.com/",
                                             kRemoteIndex])];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that if all passwords are deleted in the list view, the disabled Edit
// button replaces the Done button.
- (void)testEditButtonUpdateOnDeletion {
  // Save a password to be deleted later.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  TapEdit();

  // Select password entry to be removed.
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteButtonAtBottom()]
      performAction:grey_tap()];

  // Verify that the Edit button is visible and disabled.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Test export flow
- (void)testExportFlow {
  // Saving a form is needed for exporting passwords.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [PasswordSettingsAppInterface setUpMockReauthenticationModuleForExport];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [[[EarlGrey selectElementWithMatcher:
                  grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                                 IDS_IOS_EXPORT_PASSWORDS),
                             grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)]
      performAction:grey_tap()];

  [GetInteractionForPasswordsExportConfirmAlert(
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_EXPORT_PASSWORDS)) performAction:grey_tap()];

  // Wait until the alerts are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  id<GREYMatcher> exportButtonStatusMatcher =
      grey_accessibilityTrait(UIAccessibilityTraitNotEnabled);

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_EXPORT_PASSWORDS)]
      assertWithMatcher:exportButtonStatusMatcher];

  if ([ChromeEarlGrey isIPadIdiom]) {
    // Tap outside the activity view to dismiss it, because it is not
    // accompanied by a "Cancel" button on iPad.
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabelId(
                       IDS_IOS_EXPORT_PASSWORDS)] performAction:grey_tap()];
  } else {
    // Tap on the "Cancel" or "X" button accompanying the activity view to
    // dismiss it.
    NSString* dismissLabel = @"Close";
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     ButtonWithAccessibilityLabel(dismissLabel),
                                     grey_not(grey_accessibilityTrait(
                                         UIAccessibilityTraitNotEnabled)),
                                     grey_interactable(), nullptr)]
        performAction:grey_tap()];
  }

  // Wait until the activity view is dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that export button is re-enabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_EXPORT_PASSWORDS)]
      assertWithMatcher:grey_not(grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled))];
}

// Test that when user types text in search field, passwords and blocked
// items are filtered out and "save passwords" switch is removed.
- (void)testSearchPasswords {
// TODO(crbug.com/1067818): Test doesn't pass on iPad device.
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"This test doesn't pass on iPad device.");
  }
#endif
  SaveExamplePasswordForms();
  SaveExampleBlockedForms();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example11.com, user1")
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordEntry(@"example12.com, user2")
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordEntry(@"exclude1.com")
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordEntry(@"exclude2.com")
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SearchTextField()]
      performAction:grey_typeText(@"2")];

  // Check that the "Save Passwords" switch is hidden.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSavePasswordSwitchTableViewId, YES)]
      assertWithMatcher:grey_nil()];

  [GetInteractionForPasswordEntry(@"example11.com, user1")
      assertWithMatcher:grey_nil()];
  [GetInteractionForPasswordEntry(@"example12.com, user2")
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordEntry(@"exclude1.com")
      assertWithMatcher:grey_nil()];
  [GetInteractionForPasswordEntry(@"exclude2.com")
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(IDS_CANCEL)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Test search and delete all passwords and blocked items.
- (void)testSearchAndDeleteAllPasswords {
  SaveExamplePasswordForms();
  SaveExampleBlockedForms();

  OpenPasswordSettings();

  // TODO(crbug.com/922511): Comment out because currently activating the search
  // bar will hide the "Edit" button in the top toolbar. Recover this when the
  // "Edit" button is moved to the bottom toolbar in the new Settings UI.
  //  [[EarlGrey selectElementWithMatcher:SearchTextField()]
  //      performAction:grey_typeText(@"u\n")];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  TapEdit();

  // Select all.
  [GetInteractionForPasswordEntry(@"example11.com, user1")
      performAction:grey_tap()];
  [GetInteractionForPasswordEntry(@"example12.com, user2")
      performAction:grey_tap()];

  [GetInteractionForPasswordEntry(@"exclude1.com") performAction:grey_tap()];
  [GetInteractionForPasswordEntry(@"exclude2.com") performAction:grey_tap()];

  // Delete them.
  [[EarlGrey selectElementWithMatcher:DeleteButtonAtBottom()]
      performAction:grey_tap()];

  // All should be gone.
  [GetInteractionForPasswordEntry(@"example11.com, user1")
      assertWithMatcher:grey_nil()];
  [GetInteractionForPasswordEntry(@"example12.com, user2")
      assertWithMatcher:grey_nil()];
  [GetInteractionForPasswordEntry(@"exclude1.com")
      assertWithMatcher:grey_nil()];
  [GetInteractionForPasswordEntry(@"exclude2.com")
      assertWithMatcher:grey_nil()];
}

// Test that user can't search passwords while in edit mode.
- (void)testCantSearchPasswordsWhileInEditMode {
  SaveExamplePasswordForms();

  OpenPasswordSettings();
  TapEdit();

  // Verify search bar is disabled.
  [[EarlGrey selectElementWithMatcher:SearchTextField()]
      assertWithMatcher:grey_not(grey_userInteractionEnabled())];
}

// Test that the user can edit a password that is part of search results.
- (void)testCanEditPasswordsFromASearch {
  SaveExamplePasswordForms();
  OpenPasswordSettings();

  // TODO(crbug.com/922511): Comment out because currently activating the search
  // bar will hide the "Edit" button in the top toolbar. Recover this when the
  // "Edit" button is moved to the bottom toolbar in the new Settings UI.
  //  [[EarlGrey selectElementWithMatcher:SearchTextField()]
  //      performAction:grey_typeText(@"2")];

  TapEdit();

  // Select password entry to be edited.
  [GetInteractionForPasswordEntry(@"example12.com, user2")
      performAction:grey_tap()];

  // Delete it
  [[EarlGrey selectElementWithMatcher:DeleteButtonAtBottom()]
      performAction:grey_tap()];

  // Filter results in nothing.
  // TODO(crbug.com/922511): Comment out because currently activating the search
  // bar will hide the "Edit" button in the top toolbar. Recover this when the
  // "Edit" button is moved to the bottom toolbar in the new Settings UI.
  //  [GetInteractionForPasswordEntry(@"example11.com, user1")
  //      assertWithMatcher:grey_nil()];
  //  [GetInteractionForPasswordEntry(@"example12.com, user2")
  //      assertWithMatcher:grey_nil()];

  // Get out of edit mode.
  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      performAction:grey_tap()];

  // Remove filter search term.
  [[EarlGrey selectElementWithMatcher:SearchTextField()]
      performAction:grey_clearText()];

  // Only password 1 should show.
  [GetInteractionForPasswordEntry(@"example11.com, user1")
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordEntry(@"example12.com, user2")
      assertWithMatcher:grey_nil()];
}

// Checks that attempts to edit a password provide appropriate feedback.
- (void)testEditPassword {
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Check the snackbar in case of successful reauthentication.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  TapEdit();

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_textFieldValue(@"concrete password")];

  // Check that empty password is not allowed, and done button is disabled.
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"")];

  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"new password")];

  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:EditConfirmationButton()]
      performAction:grey_tap()];

  TapEdit();

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_textFieldValue(@"new password")];

  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to edit a username provide appropriate feedback.
- (void)testEditUsername {
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Check the snackbar in case of successful reauthentication.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  TapEdit();

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_textFieldValue(@"concrete username")];

  // Empty username should work as well.
  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"")];

  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:EditConfirmationButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_textFieldValue(@"")];

  TapEdit();

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"new username")];

  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:EditConfirmationButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_textFieldValue(@"new username")];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [GetInteractionForPasswordEntry(@"example.com, new username")
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to edit a username to a value which is already used for
// the same domain fails.
- (void)testEditUsernameFails {
  GREYAssert(
      [PasswordSettingsAppInterface saveExamplePassword:@"concrete password"
                                               userName:@"concrete username1"
                                                 origin:@"https://example.com"],
      @"Stored form was not found in the PasswordStore results.");

  GREYAssert(
      [PasswordSettingsAppInterface saveExamplePassword:@"concrete password"
                                               userName:@"concrete username2"
                                                 origin:@"https://example.com"],
      @"Stored form was not found in the PasswordStore results.");

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username1")
      performAction:grey_tap()];

  // Check the snackbar in case of successful reauthentication.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  TapEdit();

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_textFieldValue(@"concrete username1")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_clearText()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"concrete username2")];

  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];

  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to edit a username provide appropriate feedback.
- (void)testCancelDuringEditing {
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Check the snackbar in case of successful reauthentication.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  TapEdit();

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"new password")];

  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Test that password value is unchanged.
  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_textFieldValue(@"concrete password")];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that removing multiple passwords works fine.
- (void)testRemovingMultiplePasswords {
  constexpr int kPasswordsCount = 4;

  // Send the passwords to the queue to be added to the PasswordStore.
  [PasswordSettingsAppInterface saveExamplePasswordWithCount:kPasswordsCount];

  OpenPasswordSettings();
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  TapEdit();

  for (int i = kPasswordsCount; i >= 1; i--) {
    [GetInteractionForPasswordEntry([NSString
        stringWithFormat:@"www%02d.example.com, concrete username %02d", i, i])
        performAction:grey_tap()];
  }

  [[EarlGrey selectElementWithMatcher:DeleteButton()] performAction:grey_tap()];

  // Wait until animation is over.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that saved forms header is removed.
  [[EarlGrey selectElementWithMatcher:SavedPasswordsHeaderMatcher()]
      assertWithMatcher:grey_nil()];

  // Verify that the deletion was propagated to the PasswordStore.
  GREYAssertEqual(0, [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Stored password was not removed from PasswordStore.");

  // Finally, verify that the Edit button is visible and disabled, because there
  // are no other password entries left for deletion via the "Edit" mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks the 'Add Password' button is enabled when the passwords screen is
// presented irrespective of the enable password toggle.
- (void)testToolbarAddPasswordButton {
  SaveExamplePasswordForm();
  OpenPasswordSettings();

  // Toggle the "Save Passwords" control off and back on and check that add
  // password button is disabled and enabled respectively.
  BOOL isSwitchEnabled =
      [PasswordSettingsAppInterface isCredentialsServiceEnabled];
  BOOL kExpectedState[] = {isSwitchEnabled, !isSwitchEnabled};
  for (BOOL expectedState : kExpectedState) {
    // Toggle the switch. It is located near the top, so if not interactable,
    // try scrolling up.
    [GetInteractionForListItem(
        chrome_test_util::TableViewSwitchCell(kSavePasswordSwitchTableViewId,
                                              expectedState),
        kGREYDirectionUp) performAction:TurnTableViewSwitchOn(!expectedState)];

    // Check that the switch has been modified.
    [GetInteractionForListItem(
        chrome_test_util::TableViewSwitchCell(kSavePasswordSwitchTableViewId,
                                              !expectedState),
        kGREYDirectionUp) assertWithMatcher:grey_sufficientlyVisible()];

    // Expect the button to be enabled.
    [[EarlGrey selectElementWithMatcher:AddPasswordButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}

// Checks that the "Add" button is not shown on Edit.
- (void)testNoAddButtonInEditMode {
  SaveExamplePasswordForm();
  OpenPasswordSettings();

  TapEdit();

  // Expect Add Password button to be removed.
  [[EarlGrey selectElementWithMatcher:AddPasswordButton()]
      assertWithMatcher:grey_nil()];
}

// Tests the add password flow.
- (void)testAddNewPasswordCredential {
  OpenPasswordSettings();

  // Press "Add".
  [[EarlGrey selectElementWithMatcher:AddPasswordButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Fill form.
  [[EarlGrey selectElementWithMatcher:PasswordDetailWebsite()]
      performAction:grey_replaceText(@"https://www.example.com")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"new username")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"new password")];

  // The "Add" button is enabled after site and password have been entered.
  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_enabled()];

  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      performAction:grey_tap()];

  [GetInteractionForPasswordEntry(@"example.com, new username")
      performAction:grey_tap()];

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  TapEdit();

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_textFieldValue(@"new password")];
}

// Tests that when a new credential is saved or an existing one is updated via
// the add credential flow, the VC auto scrolls to the newly created or the
// updated entry.
// TODO(crbug.com/1377079): Flaky, please re-enable once fixed.
- (void)DISABLED_testAutoScroll {
  for (int i = 0; i < 20; i++) {
    NSString* username = [NSString stringWithFormat:@"username %d", i];
    NSString* password = [NSString stringWithFormat:@"password %d", i];
    NSString* site = [NSString stringWithFormat:@"https://example%d.com", i];
    GREYAssert([PasswordSettingsAppInterface saveExamplePassword:password
                                                        userName:username
                                                          origin:site],
               @"Stored form was not found in the PasswordStore results.");
  }

  OpenPasswordSettings();

  // Press "Add".
  [[EarlGrey selectElementWithMatcher:AddPasswordButton()]
      performAction:grey_tap()];

  // Fill form.
  [[EarlGrey selectElementWithMatcher:PasswordDetailWebsite()]
      performAction:grey_replaceText(@"https://zexample.com")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"zconcrete username")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"new password")];

  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      performAction:grey_tap()];

  // The newly created credential exists.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   ButtonWithAccessibilityLabel(
                                       @"zexample.com, zconcrete username"),
                                   grey_interactable(), nil)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
}

// Tests that adding new password credential where the username and website
// matches with an existing credential results in showing a section alert for
// the existing credential.
- (void)testAddNewDuplicatedPasswordCredential {
  SaveExamplePasswordForm();

  OpenPasswordSettings();
  [PasswordSettingsAppInterface setUpMockReauthenticationModuleForExport];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  // Press "Add".
  [[EarlGrey selectElementWithMatcher:AddPasswordButton()]
      performAction:grey_tap()];

  // Fill form.
  [[EarlGrey selectElementWithMatcher:PasswordDetailWebsite()]
      performAction:grey_replaceText(@"https://example.com")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"password")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"concrete username")];

  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  [PasswordSettingsAppInterface setUpMockReauthenticationModuleForAddPassword];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [[EarlGrey selectElementWithMatcher:DuplicateCredentialViewPasswordButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"new username")];

  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:EditConfirmationButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_textFieldValue(@"new username")];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [GetInteractionForPasswordEntry(@"example.com, new username")
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the duplicate credential section alert is shown when the user adds
// a credential that has the same website as that of an existing credential
// (does not contain username).
- (void)testDuplicatedCredentialWithNoUsername {
  OpenPasswordSettings();

  [[EarlGrey selectElementWithMatcher:AddPasswordButton()]
      performAction:grey_tap()];

  // Fill form.
  [[EarlGrey selectElementWithMatcher:PasswordDetailWebsite()]
      performAction:grey_replaceText(@"https://www.example.com")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"new password")];

  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      performAction:grey_tap()];

  // Add another credential.
  [[EarlGrey selectElementWithMatcher:AddPasswordButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailWebsite()]
      performAction:grey_replaceText(@"https://www.example.com")];

  // Test that the section alert for duplicated credential is shown.
  [[EarlGrey selectElementWithMatcher:DuplicateCredentialViewPasswordButton()]
      assertWithMatcher:grey_enabled()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"new username")];

  // Test that the section alert for duplicated credential is removed.
  [[EarlGrey selectElementWithMatcher:DuplicateCredentialViewPasswordButton()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"znew password")];

  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      performAction:grey_tap()];

  [GetInteractionForPasswordEntry(@"example.com, new username")
      performAction:grey_tap()];

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  TapEdit();

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_textFieldValue(@"znew password")];
}

// Tests that the error message is shown when the top-level domain is missing
// when adding a new credential.
- (void)testTLDMissingMessage {
  OpenPasswordSettings();
  [PasswordSettingsAppInterface setUpMockReauthenticationModuleForExport];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  // Press "Add".
  [[EarlGrey selectElementWithMatcher:AddPasswordButton()]
      performAction:grey_tap()];

  // Fill form.
  [[EarlGrey selectElementWithMatcher:PasswordDetailWebsite()]
      performAction:grey_replaceText(@"example")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"password")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"concrete username")];

  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSStringF(
                     IDS_IOS_SETTINGS_PASSWORDS_MISSING_TLD_DESCRIPTION,
                     u"example.com"))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that deleting a compromised password from password issues goes back
// to the list-of-issues which doesn't display that password anymore.
- (void)testDeletePasswordIssue {
  GREYAssert([PasswordSettingsAppInterface
                 saveInsecurePassword:@"concrete password"
                             userName:@"concrete username"
                               origin:@"https://example.com"],
             @"Stored form was not found in the PasswordStore results.");

  OpenPasswordSettings();

  NSString* text = l10n_util::GetNSString(IDS_IOS_CHECK_PASSWORDS);
  NSString* detailText =
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_CHECK_PASSWORDS_COMPROMISED_COUNT, 1));

  [GetInteractionForPasswordEntry([NSString
      stringWithFormat:@"%@, %@", text, detailText]) performAction:grey_tap()];

  [GetInteractionForPasswordIssueEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteButton()] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteConfirmationButton()]
      performAction:grey_tap()];

  // Wait until the alert and the detail view are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the current view is now the list view, by locating
  // PasswordIssuesTableView.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordIssuesTableViewId)]
      assertWithMatcher:grey_notNil()];

  [GetInteractionForPasswordIssueEntry(@"example.com, concrete username")
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

- (void)testShowHidePassword {
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Check the snackbar in case of successful reauthentication.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      assertWithMatcher:grey_sufficientlyVisible()];
  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      performAction:grey_tap()];
  [GetInteractionForPasswordDetailItem(HidePasswordButton())
      assertWithMatcher:grey_sufficientlyVisible()];
  [GetInteractionForPasswordDetailItem(HidePasswordButton())
      performAction:grey_tap()];
}

// Tests that the favicons for the password managers metrics are logged
// properly when there are passwords with a favicon.
// TODO(crbug.com/1348585): Fix to re-enable.
- (void)testLogFaviconsForPasswordsMetrics {
  // Sign-in and synced user.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:base::Seconds(5)];

  // Add passwords for the user.
  SaveExamplePasswordForms();

  OpenPasswordSettings();

  // Make sure the cell is loaded properly before tapping on it.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[[EarlGrey
        selectElementWithMatcher:grey_allOf(ButtonWithAccessibilityLabel(
                                                @"example12.com, user2"),
                                            grey_sufficientlyVisible(), nil)]
           usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                    kScrollAmount)
        onElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };

  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Waiting for the cell to load");

  [GetInteractionForPasswordEntry(@"example12.com, user2")
      performAction:grey_tap()];

  // Metric: Passwords in the password manager.
  // Verify that histogram is called.
  NSError* error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@"IOS.PasswordManager.PasswordsWithFavicons.Count"];
  if (error) {
    GREYFail([error description]);
  }
  // Verify the logged value of the histogram.
  error = [MetricsAppInterface
         expectSum:2
      forHistogram:@"IOS.PasswordManager.PasswordsWithFavicons.Count"];
  if (error) {
    GREYFail([error description]);
  }

  // Metric: Passwords with a favicon (image) in the password manager.
  // Verify that histogram is called.
  error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@"IOS.PasswordManager.Favicons.Count"];
  if (error) {
    GREYFail([error description]);
  }
  // Verify the logged value of the histogram.
  error = [MetricsAppInterface expectSum:0
                            forHistogram:@"IOS.PasswordManager.Favicons.Count"];
  if (error) {
    GREYFail([error description]);
  }

  // Metric: Percentage of favicons with image.
  // Verify that histogram is called.
  error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@"IOS.PasswordManager.Favicons.Percentage"];
  if (error) {
    GREYFail([error description]);
  }
  // Verify the logged value of the histogram.
  error = [MetricsAppInterface
         expectSum:0
      forHistogram:@"IOS.PasswordManager.Favicons.Percentage"];
  if (error) {
    GREYFail([error description]);
  }
}

// Tests that the favicons for the password managers metrics are logged
// properly when there are no password.
- (void)testLogFaviconsForPasswordsMetricsNoPassword {
  OpenPasswordSettings();

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Metric: Passwords in the password manager.
  // Verify that histogram is called.
  NSError* error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@"IOS.PasswordManager.PasswordsWithFavicons.Count"];
  if (error) {
    GREYFail([error description]);
  }
  // Verify the logged value of the histogram.
  error = [MetricsAppInterface
         expectSum:0
      forHistogram:@"IOS.PasswordManager.PasswordsWithFavicons.Count"];
  if (error) {
    GREYFail([error description]);
  }

  // Metric: Percentage of favicons with image.
  // This histogram is not logged.
  error = [MetricsAppInterface
      expectTotalCount:0
          forHistogram:@"IOS.PasswordManager.Favicons.Count"];
  if (error) {
    GREYFail([error description]);
  }

  // Metric: Percentage of favicons with image.
  // This histogram is not logged.
  error = [MetricsAppInterface
      expectTotalCount:0
          forHistogram:@"IOS.PasswordManager.Favicons.Percentage"];
  if (error) {
    GREYFail([error description]);
  }
}

@end
