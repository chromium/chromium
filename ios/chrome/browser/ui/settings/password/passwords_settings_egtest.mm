// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <TargetConditionals.h>

#include <utility>

#include "base/callback.h"
#include "base/ios/ios_util.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/settings/password/passwords_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/web/public/test/element_selector.h"
#include "ui/base/l10n/l10n_util.h"

#include "ios/third_party/earl_grey2/src/CommonLib/Matcher/GREYLayoutConstraint.h"  // nogncheck

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO(crbug.com/1015113) The EG2 macro is breaking indexing for some reason
// without the trailing semicolon.  For now, disable the extra semi warning
// so Xcode indexing works for the egtest.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(PasswordSettingsAppInterface);
#pragma clang diagnostic pop

// This test complements
// password_details_collection_view_controller_unittest.mm. Very simple
// integration tests and features which are not currently unittestable should
// go here, the rest into the unittest.

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::TurnSettingsSwitchOn;

namespace {

// How many points to scroll at a time when searching for an element. Setting it
// too low means searching takes too long and the test might time out. Setting
// it too high could result in scrolling way past the searched element.
constexpr int kScrollAmount = 150;

// Returns the GREYElementInteraction* for the item on the password list with
// the given |matcher|. It scrolls in |direction| if necessary to ensure that
// the matched item is interactable.
GREYElementInteraction* GetInteractionForListItem(id<GREYMatcher> matcher,
                                                  GREYDirection direction) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(direction, kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)];
}

// Returns the GREYElementInteraction* for the cell on the password list with
// the given |username|. It scrolls down if necessary to ensure that the matched
// cell is interactable.
GREYElementInteraction* GetInteractionForPasswordEntry(NSString* username) {
  return GetInteractionForListItem(ButtonWithAccessibilityLabel(username),
                                   kGREYDirectionDown);
}

// Returns the GREYElementInteraction* for the item on the detail view
// identified with the given |matcher|. It scrolls down if necessary to ensure
// that the matched cell is interactable.
GREYElementInteraction* GetInteractionForPasswordDetailItem(
    id<GREYMatcher> matcher) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(kPasswordDetailsTableViewId)];
}

// Returns the GREYElementInteraction* for the item on the deletion alert
// identified with the given |matcher|.
GREYElementInteraction* GetInteractionForPasswordDetailDeletionAlert(
    id<GREYMatcher> matcher) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_interactable(), nil)]
      inRoot:grey_accessibilityID(kPasswordDetailsDeletionAlertViewId)];
}

// Returns the GREYElementInteraction* for the item on the deletion alert
// identified with the given |matcher|.
GREYElementInteraction* GetInteractionForPasswordsExportConfirmAlert(
    id<GREYMatcher> matcher) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_interactable(), nil)]
      inRoot:grey_accessibilityID(kPasswordsExportConfirmViewId)];
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

id<GREYMatcher> SiteHeader() {
  return grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE)),
      grey_accessibilityTrait(UIAccessibilityTraitHeader), nullptr);
}

id<GREYMatcher> UsernameHeader() {
  return grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME)),
      grey_accessibilityTrait(UIAccessibilityTraitHeader), nullptr);
}

id<GREYMatcher> PasswordHeader() {
  return grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD)),
      grey_accessibilityTrait(UIAccessibilityTraitHeader), nullptr);
}

id<GREYMatcher> FederationHeader() {
  return grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_FEDERATION)),
      grey_accessibilityTrait(UIAccessibilityTraitHeader), nullptr);
}

GREYLayoutConstraint* Below() {
  return [GREYLayoutConstraint
      layoutConstraintWithAttribute:kGREYLayoutAttributeTop
                          relatedBy:kGREYLayoutRelationGreaterThanOrEqual
               toReferenceAttribute:kGREYLayoutAttributeBottom
                         multiplier:1.0
                           constant:0.0];
}

// Matcher for the Copy site button in Password Details view.
id<GREYMatcher> CopySiteButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabel(
          [NSString stringWithFormat:@"%@: %@",
                                     l10n_util::GetNSString(
                                         IDS_IOS_SHOW_PASSWORD_VIEW_SITE),
                                     l10n_util::GetNSString(
                                         IDS_IOS_SETTINGS_SITE_COPY_BUTTON)]),
      grey_interactable(), nullptr);
}

// Matcher for the Copy username button in Password Details view.
id<GREYMatcher> CopyUsernameButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabel([NSString
          stringWithFormat:@"%@: %@",
                           l10n_util::GetNSString(
                               IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME),
                           l10n_util::GetNSString(
                               IDS_IOS_SETTINGS_USERNAME_COPY_BUTTON)]),
      grey_interactable(), nullptr);
}

// Matcher for the Copy password button in Password Details view.
id<GREYMatcher> CopyPasswordButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabel([NSString
          stringWithFormat:@"%@: %@",
                           l10n_util::GetNSString(
                               IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD),
                           l10n_util::GetNSString(
                               IDS_IOS_SETTINGS_PASSWORD_COPY_BUTTON)]),
      grey_interactable(), nullptr);
}

// Matcher for the Show password button in Password Details view.
id<GREYMatcher> ShowPasswordButton() {
  return grey_allOf(ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON)),
                    grey_interactable(), nullptr);
}

// Matcher for the Delete button in Password Details view.
id<GREYMatcher> DeleteButton() {
  return grey_allOf(ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_SETTINGS_PASSWORD_DELETE_BUTTON)),
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
                    grey_userInteractionEnabled(), nil);
}

// Matches the pop-up (call-out) menu item with accessibility label equal to the
// translated string identified by |label|.
id<GREYMatcher> PopUpMenuItemWithLabel(int label) {
  if (@available(iOS 13, *)) {
    // iOS13 reworked menu button subviews to no longer be accessibility
    // elements.  Multiple menu button subviews no longer show up as potential
    // matches, which means the matcher logic does not need to be as complex as
    // the iOS 11/12 logic.  Various table view cells may share the same
    // accesibility label, but those can be filtered out by ignoring
    // UIAccessibilityTraitButton.
    return grey_allOf(
        grey_accessibilityLabel(l10n_util::GetNSString(label)),
        grey_not(grey_accessibilityTrait(UIAccessibilityTraitButton)), nil);
  } else {
    // This is a hack relying on UIKit's internal structure. There are multiple
    // items with the label the test is looking for, because the menu items
    // likely have the same labels as the buttons for the same function. There
    // is no easy way to identify elements which are part of the pop-up, because
    // the associated classes are internal to UIKit. However, the pop-up items
    // are of internal classs UICalloutBarButton, which can be tested easily
    // in EG2.
    return grey_allOf(grey_kindOfClassName(@"UICalloutBarButton"),
                      grey_accessibilityLabel(l10n_util::GetNSString(label)),
                      nullptr);
  }
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

}  // namespace

// Various tests for the Save Passwords section of the settings.
@interface PasswordsSettingsTestCase : ChromeTestCase
@end

@implementation PasswordsSettingsTestCase

- (void)tearDown {
  // Snackbars triggered by tests stay up for a limited time even if the
  // settings get closed. Ensure that they are closed to avoid interference with
  // other tests.
  [PasswordSettingsAppInterface dismissSnackBar];
  GREYAssert([PasswordSettingsAppInterface clearPasswordStore],
             @"PasswordStore was not cleared.");

  [super tearDown];
}

// Verifies the UI elements are accessible on the Passwords page.
- (void)testAccessibilityOnPasswords {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  TapEdit();
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
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

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  // Check the snackbar in case of successful reauthentication.
  [GetInteractionForPasswordDetailItem(CopyPasswordButton())
      performAction:grey_tap()];

  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  // Check the snackbar in case of failed reauthentication.
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kFailure];
  [GetInteractionForPasswordDetailItem(CopyPasswordButton())
      performAction:grey_tap()];

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
- (void)testShowPasswordToastAuthSucceeded {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  // Check the snackbar in case of successful reauthentication.
  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      performAction:grey_tap()];

  // Check that the password is displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"concrete password")]
      assertWithMatcher:grey_sufficientlyVisible()];

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
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"concrete password")]
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

  [GetInteractionForPasswordDetailItem(CopyUsernameButton())
      performAction:grey_tap()];
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

  [GetInteractionForPasswordDetailItem(CopySiteButton())
      performAction:grey_tap()];
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

  [GetInteractionForPasswordDetailItem(DeleteButton())
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailDeletionAlert(ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_CONFIRM_PASSWORD_DELETION)))
      performAction:grey_tap()];

  // Wait until the alert and the detail view are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the current view is now the list view, by locating the header
  // of the list of passwords.
  [[EarlGrey selectElementWithMatcher:SavedPasswordsHeaderMatcher()]
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
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(),
                                   grey_not(grey_enabled()), nil)];

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

  [GetInteractionForPasswordDetailItem(DeleteButton())
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailDeletionAlert(ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_CONFIRM_PASSWORD_DELETION)))
      performAction:grey_tap()];

  // Wait until the alert and the detail view are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the current view is now the list view, by locating the header
  // of the list of passwords.
  [[EarlGrey selectElementWithMatcher:SavedPasswordsHeaderMatcher()]
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
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(),
                                   grey_not(grey_enabled()), nil)];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that deleting a blocked form from password details view goes
// back to the list-of-passwords view which doesn't display that form anymore.
- (void)testBlockedFormDeletionInDetailView {
  // Save blacklisted form to be deleted later.
  GREYAssert([PasswordSettingsAppInterface
                 saveExampleBlockedOrigin:@"https://blocked.com"],
             @"Stored form was not found in the PasswordStore results.");

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"blocked.com") performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(DeleteButton())
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailDeletionAlert(ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_CONFIRM_PASSWORD_DELETION)))
      performAction:grey_tap()];

  // Wait until the alert and the detail view are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the current view is now the list view, by locating the header
  // of the list of passwords.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_SETTINGS_PASSWORDS_EXCEPTIONS_HEADING)),
                            grey_accessibilityTrait(UIAccessibilityTraitHeader),
                            nullptr)] assertWithMatcher:grey_notNil()];

  // Verify that the deletion was propagated to the PasswordStore.
  GREYAssertEqual(0, [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Stored password was not removed from PasswordStore.");

  // Also verify that the removed password is no longer in the list.
  [GetInteractionForPasswordEntry(@"secret.com")
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Finally, verify that the Edit button is visible and disabled, because there
  // are no other password entries left for deletion via the "Edit" mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(),
                                   grey_not(grey_enabled()), nil)];

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

  [GetInteractionForPasswordDetailItem(DeleteButton())
      performAction:grey_tap()];

  // Tap the alert's Cancel button to cancel.
  if (base::ios::IsRunningOnOrLater(13, 2, 0) && [ChromeEarlGrey isIPadIdiom]) {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kPasswordDetailsTableViewId)]
        performAction:grey_tap()];
  } else {
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     ButtonWithAccessibilityLabel(
                                         l10n_util::GetNSString(
                                             IDS_IOS_CANCEL_PASSWORD_DELETION)),
                                     grey_interactable(), nullptr)]
        performAction:grey_tap()];
  }

  // Check that the current view is still the detail view, by locating the Copy
  // button.
  [[EarlGrey selectElementWithMatcher:CopyPasswordButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the deletion did not happen.
  GREYAssertEqual(1u, [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Stored password was removed from PasswordStore.");

  // Go back to the list view and verify that the password is still in the
  // list.
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
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                          kSavePasswordSwitchTableViewId, YES,
                                          NO)] assertWithMatcher:grey_notNil()];

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Check that the current view is not the detail view, by failing to locate
  // the Copy button.
  [[EarlGrey selectElementWithMatcher:CopyPasswordButton()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to copy the site via the context menu item provide an
// appropriate feedback.
- (void)testCopySiteMenuItem {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Tap the site cell to display the context menu.
  [GetInteractionForPasswordDetailItem(grey_accessibilityLabel(
      @"https://example.com/")) performAction:grey_tap()];

  // Tap the context menu item for copying.
  [[EarlGrey selectElementWithMatcher:PopUpMenuItemWithLabel(
                                          IDS_IOS_SETTINGS_SITE_COPY_MENU_ITEM)]
      performAction:grey_tap()];

  // Check the snackbar.
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

// Checks that attempts to copy the username via the context menu item provide
// an appropriate feedback.
- (void)testCopyUsernameMenuItem {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Tap the username cell to display the context menu.
  [GetInteractionForPasswordDetailItem(
      grey_accessibilityLabel(@"concrete username")) performAction:grey_tap()];

  // Tap the context menu item for copying.
  [[EarlGrey
      selectElementWithMatcher:PopUpMenuItemWithLabel(
                                   IDS_IOS_SETTINGS_USERNAME_COPY_MENU_ITEM)]
      performAction:grey_tap()];

  // Check the snackbar.
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

// Checks that attempts to copy the password via the context menu item provide
// an appropriate feedback.
- (void)testCopyPasswordMenuItem {
  if (![ChromeEarlGrey isIPadIdiom]) {
    // TODO(crbug.com/1109644): Enable the test on iPhone once the bug is fixed.
    EARL_GREY_TEST_DISABLED(@"Disabled for iPhone.");
  }

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

// Checks that attempts to show and hide the password via the context menu item
// provide an appropriate feedback.
- (void)testShowHidePasswordMenuItem {
  if (![ChromeEarlGrey isIPadIdiom]) {
    // TODO(crbug.com/1109644): Enable the test on iPhone once the bug is fixed.
    EARL_GREY_TEST_DISABLED(@"Disabled for iPhone.");
  }

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

  // Tap the context menu item for showing.
  [[EarlGrey
      selectElementWithMatcher:PopUpMenuItemWithLabel(
                                   IDS_IOS_SETTINGS_PASSWORD_SHOW_MENU_ITEM)]
      performAction:grey_tap()];

  // Tap the password cell to display the context menu again, and to check that
  // the password was unmasked.
  [GetInteractionForPasswordDetailItem(
      grey_accessibilityLabel(@"concrete password")) performAction:grey_tap()];

  // Tap the context menu item for hiding.
  [[EarlGrey
      selectElementWithMatcher:PopUpMenuItemWithLabel(
                                   IDS_IOS_SETTINGS_PASSWORD_HIDE_MENU_ITEM)]
      performAction:grey_tap()];

  // Check that the password is masked again.
  [GetInteractionForPasswordDetailItem(grey_text(kMaskedPassword))
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

  // Check that the Site, Username, Federation and Delete Saved Password
  // sections are there.
  [GetInteractionForPasswordDetailItem(SiteHeader())
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordDetailItem(UsernameHeader())
      assertWithMatcher:grey_notNil()];
  // For federation check both the section header and content.
  [GetInteractionForPasswordDetailItem(FederationHeader())
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordDetailItem(grey_text(@"famous.provider.net"))
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordDetailItem(DeleteButton())
      assertWithMatcher:grey_notNil()];

  // Check that the password is not present.
  [GetInteractionForPasswordDetailItem(PasswordHeader())
      assertWithMatcher:grey_nil()];

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

  [GetInteractionForPasswordDetailItem(SiteHeader())
      assertWithMatcher:grey_notNil()];
  id<GREYMatcher> siteCell = grey_accessibilityLabel(@"https://example.com/");
  [GetInteractionForPasswordDetailItem(siteCell)
      assertWithMatcher:grey_layout(@[ Below() ], SiteHeader())];
  [GetInteractionForPasswordDetailItem(CopySiteButton())
      assertWithMatcher:grey_layout(@[ Below() ], siteCell)];

  [GetInteractionForPasswordDetailItem(UsernameHeader())
      assertWithMatcher:grey_layout(@[ Below() ], CopySiteButton())];
  id<GREYMatcher> usernameCell = grey_accessibilityLabel(@"concrete username");
  [GetInteractionForPasswordDetailItem(usernameCell)
      assertWithMatcher:grey_layout(@[ Below() ], UsernameHeader())];
  [GetInteractionForPasswordDetailItem(CopyUsernameButton())
      assertWithMatcher:grey_layout(@[ Below() ], usernameCell)];

  id<GREYMatcher> passwordHeader =
      grey_allOf(PasswordHeader(),
                 grey_kindOfClassName(@"UITableViewHeaderFooterView"), nil);
  [GetInteractionForPasswordDetailItem(passwordHeader)
      assertWithMatcher:grey_layout(@[ Below() ], CopyUsernameButton())];
  id<GREYMatcher> passwordCell = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDDEN_LABEL));
  [GetInteractionForPasswordDetailItem(passwordCell)
      assertWithMatcher:grey_layout(@[ Below() ], passwordHeader)];
  [GetInteractionForPasswordDetailItem(CopyPasswordButton())
      assertWithMatcher:grey_layout(@[ Below() ], passwordCell)];
  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      assertWithMatcher:grey_layout(@[ Below() ], CopyPasswordButton())];

  [GetInteractionForPasswordDetailItem(DeleteButton())
      assertWithMatcher:grey_layout(@[ Below() ], ShowPasswordButton())];

  // Check that the federation block is not present. Match directly to also
  // catch the case where the block would be present but not currently visible
  // due to the scrolling state.
  [[EarlGrey selectElementWithMatcher:FederationHeader()]
      assertWithMatcher:grey_nil()];

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

  [GetInteractionForPasswordDetailItem(SiteHeader())
      assertWithMatcher:grey_notNil()];
  id<GREYMatcher> siteCell = grey_accessibilityLabel(@"https://example.com/");
  [GetInteractionForPasswordDetailItem(siteCell)
      assertWithMatcher:grey_layout(@[ Below() ], SiteHeader())];
  [GetInteractionForPasswordDetailItem(CopySiteButton())
      assertWithMatcher:grey_layout(@[ Below() ], siteCell)];

  [GetInteractionForPasswordDetailItem(DeleteButton())
      assertWithMatcher:grey_layout(@[ Below() ], CopySiteButton())];

  // Check that the other blocks are not present. Match directly to also catch
  // the case where those blocks would be present but not currently visible due
  // to the scrolling state.
  [[EarlGrey selectElementWithMatcher:UsernameHeader()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:PasswordHeader()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:FederationHeader()]
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

  [GetInteractionForPasswordDetailItem(SiteHeader())
      assertWithMatcher:grey_notNil()];
  id<GREYMatcher> siteCell = grey_accessibilityLabel(@"https://example.com/");
  [GetInteractionForPasswordDetailItem(siteCell)
      assertWithMatcher:grey_layout(@[ Below() ], SiteHeader())];
  [GetInteractionForPasswordDetailItem(CopySiteButton())
      assertWithMatcher:grey_layout(@[ Below() ], siteCell)];

  [GetInteractionForPasswordDetailItem(UsernameHeader())
      assertWithMatcher:grey_layout(@[ Below() ], CopySiteButton())];
  id<GREYMatcher> usernameCell = grey_accessibilityLabel(@"federated username");
  [GetInteractionForPasswordDetailItem(usernameCell)
      assertWithMatcher:grey_layout(@[ Below() ], UsernameHeader())];
  [GetInteractionForPasswordDetailItem(CopyUsernameButton())
      assertWithMatcher:grey_layout(@[ Below() ], usernameCell)];

  [GetInteractionForPasswordDetailItem(FederationHeader())
      assertWithMatcher:grey_layout(@[ Below() ], CopyUsernameButton())];
  id<GREYMatcher> federationCell = grey_text(@"famous.provider.net");
  [GetInteractionForPasswordDetailItem(federationCell)
      assertWithMatcher:grey_layout(@[ Below() ], FederationHeader())];

  [GetInteractionForPasswordDetailItem(DeleteButton())
      assertWithMatcher:grey_layout(@[ Below() ], federationCell)];

  // Check that the password is not present. Match directly to also catch the
  // case where the password header would be present but not currently visible
  // due to the scrolling state.
  [[EarlGrey selectElementWithMatcher:PasswordHeader()]
      assertWithMatcher:grey_nil()];

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

  GREYAssert([PasswordSettingsAppInterface
                 saveExampleBlockedOrigin:@"https://blocked.com"],
             @"Stored form was not found in the PasswordStore results.");

  OpenPasswordSettings();

  // Toggle the "Save Passwords" control off and back on and check that stored
  // items are still present.
  constexpr BOOL kExpectedState[] = {YES, NO};
  for (BOOL expected_state : kExpectedState) {
    // Toggle the switch. It is located near the top, so if not interactable,
    // try scrolling up.
    [GetInteractionForListItem(
        chrome_test_util::SettingsSwitchCell(kSavePasswordSwitchTableViewId,
                                             expected_state),
        kGREYDirectionUp) performAction:TurnSettingsSwitchOn(!expected_state)];

    // Check that the switch has been modified.
    [GetInteractionForListItem(
        chrome_test_util::SettingsSwitchCell(kSavePasswordSwitchTableViewId,
                                             !expected_state),
        kGREYDirectionUp) assertWithMatcher:grey_sufficientlyVisible()];

    // Check the stored items. Scroll down if needed.
    [GetInteractionForPasswordEntry(@"example.com, concrete username")
        assertWithMatcher:grey_notNil()];
    [GetInteractionForPasswordEntry(@"blocked.com")
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
    [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                            kSavePasswordSwitchTableViewId,
                                            expected_initial_state)]
        performAction:TurnSettingsSwitchOn(!expected_initial_state)];
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

  [GetInteractionForPasswordDetailItem(CopyPasswordButton())
      performAction:grey_tap()];

  NSString* title =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(title)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OKButton()]
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
  constexpr int kRemoteIndex = kPasswordsCount - 2;
  // The scrolling in GetInteractionForPasswordEntry has too fine steps to
  // reach the desired part of the list quickly. The following gives it a head
  // start of almost the desired position, counting 30 points per entry and
  // aiming 3 entries before |kRemoteIndex|.
  constexpr int kJump = (kRemoteIndex - 3) * 30;
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)]
      performAction:grey_scrollInDirection(kGREYDirectionDown, kJump)];
  [GetInteractionForPasswordEntry([NSString
      stringWithFormat:@"www%02d.example.com, concrete username %02d",
                       kRemoteIndex, kRemoteIndex]) performAction:grey_tap()];

  // Check that the detail view loaded correctly by verifying the site content.
  id<GREYMatcher> siteCell = grey_accessibilityLabel([NSString
      stringWithFormat:@"https://www%02d.example.com/", kRemoteIndex]);
  [GetInteractionForPasswordDetailItem(siteCell)
      assertWithMatcher:grey_notNil()];

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
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(),
                                   grey_not(grey_enabled()), nil)];

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

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_EXPORT_PASSWORDS)]
      performAction:grey_tap()];

  [GetInteractionForPasswordsExportConfirmAlert(
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_EXPORT_PASSWORDS)) performAction:grey_tap()];

  // Wait until the alerts are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // On iOS 13+ phone when building with the iOS 12 SDK, the share sheet is
  // presented fullscreen, so the export button is removed from the view
  // hierarchy.  Check that either the button is not present, or that it remains
  // visible but is disabled.
  id<GREYMatcher> exportButtonStatusMatcher =
      grey_accessibilityTrait(UIAccessibilityTraitNotEnabled);
#if !defined(__IPHONE_13_0) || (__IPHONE_OS_VERSION_MAX_ALLOWED < __IPHONE_13_0)
  if (base::ios::IsRunningOnIOS13OrLater()) {
    exportButtonStatusMatcher =
        grey_anyOf(grey_nil(), exportButtonStatusMatcher, nil);
  }
#endif

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
    NSString* dismissLabel =
        base::ios::IsRunningOnIOS13OrLater() ? @"Close" : @"Cancel";
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     ButtonWithAccessibilityLabel(dismissLabel),
                                     grey_interactable(),
                                     grey_not(grey_accessibilityTrait(
                                         UIAccessibilityTraitNotEnabled)),
                                     nullptr)] performAction:grey_tap()];
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
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
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
}

// Test search and delete all passwords and blocked items.
- (void)testSearchAndDeleteAllPasswords {
  // TODO(crbug.com/1129441): This is failing regularly downstream on iOS14.
  if (@available(iOS 14, *)) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS14.");
  }

  SaveExamplePasswordForms();
  SaveExampleBlockedForms();

  OpenPasswordSettings();

  // TODO(crbug.com/922511): Comment out because currently activating the search
  // bar will hide the "Edit" button in the top toolbar. Recover this when the
  // "Edit" button is moved to the bottom toolbar in the new Settings UI.
  //  [[EarlGrey selectElementWithMatcher:SearchTextField()]
  //      performAction:grey_typeText(@"u\n")];

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
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
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

@end
