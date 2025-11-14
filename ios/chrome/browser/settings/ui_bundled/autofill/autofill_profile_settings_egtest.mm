// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/common/features.h"
#import "components/policy/policy_constants.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_constants.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_constants.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_egtest_util.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SearchBar;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SettingsToolbarAddButton;
using chrome_test_util::SettingsToolbarEditButton;
using chrome_test_util::TabGridEditButton;
using policy_test_utils::SetPolicy;

namespace {

// Will be used to test the country selection logic.
NSString* const kCountryForSelection = @"Germany";

constexpr base::TimeDelta kSnackbarAppearanceTimeout = base::Seconds(5);

NSString* const kProfileLabel = @"John H. Doe, 666 Erebus St.";
NSString* const kHomeProfileLabel = @"John H. Doe, 666 Erebus St., Home";

// Return the edit button from the navigation bar.
id<GREYMatcher> NavigationBarEditButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
      grey_not(TabGridEditButton()),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

// Matcher for a country entry with the given accessibility label.
id<GREYMatcher> CountryEntry(NSString* label) {
  return grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(label),
                    grey_userInteractionEnabled(), grey_sufficientlyVisible(),
                    nil);
}

// Matcher for the "Country" button.
id<GREYMatcher> CountryButton() {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_IOS_AUTOFILL_COUNTRY)),
      grey_userInteractionEnabled(), nil);
}

// Matcher for the search bar's scrim.
id<GREYMatcher> SearchBarScrim() {
  return grey_accessibilityID(kAutofillCountrySelectionSearchScrimId);
}

// Matcher for migrate to account button.
id<GREYMatcher> MigrateToAccountButton() {
  return grey_accessibilityID(kAutofillAddressMigrateToAccountButtonId);
}

id<GREYMatcher> EditCellButton() {
  return grey_accessibilityID(kAutofillEditButtonCellId);
}

// Matcher for the navigation bar title of the "Adresses and more" page.
id<GREYMatcher> AddressesAndMoreNavBarTitle() {
  return grey_allOf(
      grey_text(l10n_util::GetNSString(IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE)),
      grey_kindOfClass([UILabel class]),
      grey_ancestor(grey_kindOfClass([UINavigationBar class])), nil);
}

// Matcher for the toolbar's done button.
id<GREYMatcher> SettingsToolbarDoneButton() {
  return grey_accessibilityID(kSettingsToolbarEditDoneButtonId);
}

// Matcher to text field with label `textFieldLabel`.
id<GREYMatcher> TextFieldWithLabel(NSString* textFieldLabel) {
  return grey_allOf(grey_accessibilityID(
                        [textFieldLabel stringByAppendingString:@"_textField"]),
                    grey_kindOfClass([UITextField class]), nil);
}

}  // namespace

// Various tests for the Autofill profiles section of the settings.
@interface AutofillProfileSettingsTestCase : ChromeTestCase
@end

@implementation AutofillProfileSettingsTestCase

- (void)setUp {
  [super setUp];
  [AutofillAppInterface clearProfilesStore];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  if ([self isRunningTest:@selector(testHomeAndWorkProfileEditPage)] ||
      [self isRunningTest:@selector(testHomeAndWorkProfileDeleteOnEdit)] ||
      [self isRunningTest:@selector(testHomeAndWorkProfileRemove)] ||
      [self isRunningTest:@selector(testConfirmationShownOnDeletion)] ||
      [self isRunningTest:@selector(testConfirmationShownOnSwipeToDelete)]) {
    config.features_enabled.push_back(
        autofill::features::kAutofillEnableSupportForHomeAndWork);
  }

  return config;
}

- (void)tearDownHelper {
  [AutofillAppInterface clearProfilesStore];
  [super tearDownHelper];
}

// Helper to open the settings page for Autofill profiles.
- (void)openAutofillProfilesSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::AddressesAndMoreButton()];
}

// Helper to open the settings page for the Autofill profile with `label`.
- (void)openEditProfile:(NSString*)label {
  [self openAutofillProfilesSettings];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(label)]
      performAction:grey_tap()];
}

// Close the settings.
- (void)exitSettingsMenu {
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  // Wait for UI components to finish loading.
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Scroll to the bottom.
- (void)scrollDownWithMatcher:(id<GREYMatcher>)scrollViewMatcher {
  [[EarlGrey selectElementWithMatcher:scrollViewMatcher]
      performAction:grey_scrollToContentEdgeWithStartPoint(
                        kGREYContentEdgeBottom, 0.1f, 0.1f)];
}

// Helper to open the settings page for the Autofill profile card list in edit
// mode.
- (void)openProfileListInEditMode {
  [self openAutofillProfilesSettings];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
}

// Returns the matcher for the delete button in the deletion confirmation sheet.
- (id<GREYMatcher>)confirmButtonForDeleteAddress {
  id<GREYMatcher> baseMatcher = grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_SETTINGS_AUTOFILL_DELETE_ADDRESSES_CONFIRMATION_BUTTON)),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_userInteractionEnabled(), nil);

  return grey_allOf(baseMatcher, grey_not(grey_descendant(baseMatcher)), nil);
}

// Returns the matcher for the remove button in the home/work address deletion
// confirmation sheet.
- (id<GREYMatcher>)confirmButtonForRemoveAddress {
  id<GREYMatcher> baseMatcher = grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_SETTINGS_AUTOFILL_REMOVE_ADDRESS_CONFIRMATION_BUTTON)),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_userInteractionEnabled(), nil);

  return grey_allOf(baseMatcher, grey_not(grey_descendant(baseMatcher)), nil);
}

// Returns the matcher for the edit button in the home/work address deletion
// confirmation sheet.
- (id<GREYMatcher>)editButtonInAddressDeletionConfirmationSheet {
  id<GREYMatcher> baseMatcher = grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_SETTINGS_AUTOFILL_EDIT_HOME_WORK_ADDRESS_CONFIRMATION_BUTTON)),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_userInteractionEnabled(), nil);

  return grey_allOf(baseMatcher, grey_not(grey_descendant(baseMatcher)), nil);
}

// Returns the footer based on the count of errors due to the empty required
// fields.
- (id<GREYMatcher>)footerWithCountOfEmptyRequiredFields:(int)countOfrrors {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetPluralNSStringF(
          IDS_IOS_SETTINGS_EDIT_AUTOFILL_ADDRESS_REQUIREMENT_ERROR,
          countOfrrors)),
      grey_sufficientlyVisible(), nil);
}

// Test that the page for viewing Autofill profile details is accessible.
// TODO(crbug.com/413107639): Test is flaky.
- (void)FLAKY_testAccessibilityOnAutofillProfileViewPage {
  [AutofillAppInterface saveExampleProfile];
  [self openEditProfile:kProfileLabel];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Test that the page for editing Autofill profile details is accessible.
- (void)testAccessibilityOnAutofillProfileEditPage {
  [AutofillAppInterface saveExampleProfile];
  [self openEditProfile:kProfileLabel];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Test that the edit mode for Home and Work profiles is not accessible.
- (void)testHomeAndWorkProfileEditPage {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [AutofillAppInterface saveExampleHomeAndWorkAccountProfile];
  [self openEditProfile:kHomeProfileLabel];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:EditCellButton()]
      performAction:grey_tap()];

  // Assert that the edit page is no longer displayed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAutofillProfileEditTableViewId)]
      assertWithMatcher:grey_nil()];

  [SigninEarlGrey signOut];
}

// Checks that the Autofill profiles list view is in edit mode and the Autofill
// profiles switch is disabled.
- (void)testListViewEditMode {
  [AutofillAppInterface saveExampleProfile];
  [self openProfileListInEditMode];

  // Check the Autofill profile switch is disabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillAddressSwitchViewId,
                                   /*is_toggled_on=*/YES, /*is_enabled=*/NO)]
      assertWithMatcher:grey_notNil()];

  [self exitSettingsMenu];
}

// Checks that the toolbar "Add" button is only visible when the table view is
// not in edit mode.
- (void)testBottomToolbarAddButtonVisibility {
  [AutofillAppInterface saveExampleProfile];
  [self openAutofillProfilesSettings];

  // Verify the "Add" button is initially visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SettingsToolbarAddButton()];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:SettingsToolbarEditButton()]
      performAction:grey_tap()];

  // Confirm that the "Add" button no longer exists.
  [ChromeEarlGrey waitForNotSufficientlyVisibleElementWithMatcher:
                      SettingsToolbarAddButton()];

  // Switch off edit mode.
  [[EarlGrey selectElementWithMatcher:SettingsToolbarDoneButton()]
      performAction:grey_tap()];

  // Verify the "Add" button is visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SettingsToolbarAddButton()];
}

// Checks that the toolbar "Add" button's enabled state changes based on the
// "Autofill profiles" switch.
- (void)testToggleToolbarAddButtonBySwitch {
  [AutofillAppInterface saveExampleProfile];
  [self openAutofillProfilesSettings];

  // Toggle the "Autofill profiles" switch off.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillAddressSwitchViewId,
                                   /*is_toggled_on=*/YES, /*is_enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Verify the "Add" button is disabled.
  [[EarlGrey selectElementWithMatcher:SettingsToolbarAddButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Toggle the "Autofill profiles" switch back on.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillAddressSwitchViewId,
                                   /*is_toggled_on=*/NO, /*is_enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

  // Verify the "Add" button is enabled.
  [[EarlGrey selectElementWithMatcher:SettingsToolbarAddButton()]
      assertWithMatcher:grey_enabled()];
}

// Checks that the toolbar "Add" button's enabled state changes based on the
// AutofillAddressEnabled Enterprise Policy.
- (void)testToggleToolbarAddButtonByPolicy {
  // Force the preference off via policy.
  SetPolicy(false, policy::key::kAutofillAddressEnabled);

  [self openAutofillProfilesSettings];

  // Verify the "Add" button is disabled.
  [[EarlGrey selectElementWithMatcher:SettingsToolbarAddButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  [self exitSettingsMenu];

  // Force the preference on via policy.
  SetPolicy(true, policy::key::kAutofillAddressEnabled);

  [self openAutofillProfilesSettings];

  // Verify the "Add" button is enabled.
  [[EarlGrey selectElementWithMatcher:SettingsToolbarAddButton()]
      assertWithMatcher:grey_enabled()];
}

// Checks that the Autofill profile switch can be toggled on/off and the list of
// Autofill profiles is not affected by it.
- (void)testToggleAutofillProfileSwitch {
  [AutofillAppInterface saveExampleProfile];
  [self openAutofillProfilesSettings];

  // Toggle the Autofill profiles switch off.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillAddressSwitchViewId,
                                   /*is_toggled_on=*/YES, /*is_enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Expect Autofill profiles to remain visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(kProfileLabel)]
      assertWithMatcher:grey_notNil()];

  // Toggle the Autofill profiles switch back on.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillAddressSwitchViewId,
                                   /*is_toggled_on=*/NO, /*is_enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

  // Expect Autofill profiles to remain visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(kProfileLabel)]
      assertWithMatcher:grey_notNil()];

  [self exitSettingsMenu];
}

// Checks that the confirmation action sheet is shown when an autofill profile
// is deleted and the profile is deleted when the confirmation is accepted.
- (void)testConfirmationShownOnDeletion {
  [AutofillAppInterface saveExampleProfile];
  [self openProfileListInEditMode];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   [AutofillAppInterface exampleProfileName])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:[self confirmButtonForDeleteAddress]]
      performAction:grey_tap()];
  WaitForActivityOverlayToDisappear();

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      assertWithMatcher:grey_nil()];
  // If the done button in the nav bar is enabled it is no longer in edit
  // mode.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that the confirmation action sheet is shown when an autofill profile
// is swiped to be deleted.
- (void)testConfirmationShownOnSwipeToDelete {
  [AutofillAppInterface saveExampleProfile];
  [self openAutofillProfilesSettings];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   [AutofillAppInterface exampleProfileName])]
      performAction:chrome_test_util::SwipeToShowDeleteButton()];

  // There are multiple delete buttons but only one enabled.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   ButtonWithAccessibilityLabel(@"Delete"),
                                   grey_not(grey_accessibilityTrait(
                                       UIAccessibilityTraitNotEnabled)),
                                   nil)] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:[self confirmButtonForDeleteAddress]]
      performAction:grey_tap()];
  WaitForActivityOverlayToDisappear();

  // Check the profile has been deleted.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   [AutofillAppInterface exampleProfileName])]
      assertWithMatcher:grey_notVisible()];
}

// Checks that the country field is a selection field in the edit mode and the
// newly selected country gets saved in the profile.
- (void)testCountrySelection {
  [AutofillAppInterface saveExampleProfile];
  [self openEditProfile:kProfileLabel];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:CountryButton()]
      performAction:grey_tap()];

  // Focus the search bar.
  [[EarlGrey selectElementWithMatcher:SearchBar()] performAction:grey_tap()];

  // Verify the scrim is visible when search bar is focused but not typed in.
  [[EarlGrey selectElementWithMatcher:SearchBarScrim()]
      assertWithMatcher:grey_notNil()];

  // Verify the cancel button is visible and unfocuses search bar when tapped.
  [ChromeEarlGreyUI clearAndDismissSearchBar];

  // Verify countries are searchable using their name in the current locale.
  [[EarlGrey selectElementWithMatcher:SearchBar()] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SearchBar()]
      performAction:grey_replaceText(kCountryForSelection)];

  // Verify that scrim is not visible anymore.
  [[EarlGrey selectElementWithMatcher:SearchBarScrim()]
      assertWithMatcher:grey_nil()];

  // Verify the `kCountryForSelection` country is visible.
  [[EarlGrey selectElementWithMatcher:CountryEntry(kCountryForSelection)]
      assertWithMatcher:grey_notNil()];

  // Tap on `kCountryForSelection`.
  [[EarlGrey selectElementWithMatcher:CountryEntry(kCountryForSelection)]
      performAction:grey_tap()];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  // Open the profile again.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(kProfileLabel)]
      performAction:grey_tap()];

  // Check `kCountryForSelection` is saved.
  [[EarlGrey selectElementWithMatcher:grey_text(kCountryForSelection)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  // Wait for the "Addresses and more" page before exiting the Settings menu
  // using the navigation back button. This is to avoid a racing condition where
  // the back button matcher is determined before the settings page actually
  // changed which ends up picking a matcher for the previous page instead of
  // the new page to be loaded.
  ConditionBlock wait_for_appearance = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:AddressesAndMoreNavBarTitle()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(1),
                                                          wait_for_appearance),
             @"\"Addresses and more\" page did not appear.");

  [self exitSettingsMenu];
}

// Checks when the country field is changed to Germany in the edit mode, the
// city is added to the required fields. When it is emptied, the save button in
// displayed. The profile is an account profile.
- (void)testRequiredFields {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [AutofillAppInterface saveExampleAccountProfile];
  [self openEditProfile:kProfileLabel];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:CountryButton()]
      performAction:grey_tap()];

  // Focus the search bar.
  [[EarlGrey selectElementWithMatcher:SearchBar()] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SearchBar()]
      performAction:grey_replaceText(kCountryForSelection)];

  // Verify that scrim is not visible anymore.
  [[EarlGrey selectElementWithMatcher:SearchBarScrim()]
      assertWithMatcher:grey_nil()];

  // Verify the `kCountryForSelection` country is visible.
  [[EarlGrey selectElementWithMatcher:CountryEntry(kCountryForSelection)]
      assertWithMatcher:grey_notNil()];

  // Tap on `kCountryForSelection`.
  [[EarlGrey selectElementWithMatcher:CountryEntry(kCountryForSelection)]
      performAction:grey_tap()];

  // Remove the text from the state field.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(@"State")]
      performAction:grey_replaceText(@"")];

  // The "Done" button is still visible as the state field is not a required
  // field.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Remove the text from the city field.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(@"City")]
      performAction:grey_replaceText(@"")];

  // The "Done" button is not enabled now.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  [self exitSettingsMenu];
  [SigninEarlGrey signOut];
}

// Tests that when country selection view opens, the currently selected country
// is in view.
- (void)testAutoScrollInCountrySelector {
  [AutofillAppInterface saveExampleProfile];
  [self openEditProfile:kProfileLabel];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:CountryButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:CountryEntry(@"United States")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that when the state data is removed, the "Done" button is enabled for
// "Germany" but not for "India". Similarly, the "Done" is disabled for "US".
- (void)testDoneButtonByRequirementsOfCountries {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [AutofillAppInterface saveExampleAccountProfile];
  [self openEditProfile:kProfileLabel];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  // Change text of state to empty.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(@"State")]
      performAction:grey_replaceText(@"")];

  // The "Done" button should not be enabled now since "State" is a required
  // field for "United States".
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  [[EarlGrey selectElementWithMatcher:CountryButton()]
      performAction:grey_tap()];

  // Focus the search bar.
  [[EarlGrey selectElementWithMatcher:SearchBar()] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SearchBar()]
      performAction:grey_replaceText(kCountryForSelection)];

  // Verify the `kCountryForSelection` country is visible.
  [[EarlGrey selectElementWithMatcher:CountryEntry(kCountryForSelection)]
      assertWithMatcher:grey_notNil()];

  // Tap on `kCountryForSelection`.
  [[EarlGrey selectElementWithMatcher:CountryEntry(kCountryForSelection)]
      performAction:grey_tap()];

  // The "Done" button should be enabled since "State" is not a required field
  // for "Germany".
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_enabled()];

  // Tap on Country and select "India" now.
  [[EarlGrey selectElementWithMatcher:CountryButton()]
      performAction:grey_tap()];

  // Focus the search bar.
  [[EarlGrey selectElementWithMatcher:SearchBar()] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SearchBar()]
      performAction:grey_replaceText(@"India")];

  // Verify the "India" is visible.
  [[EarlGrey selectElementWithMatcher:CountryEntry(@"India")]
      assertWithMatcher:grey_notNil()];

  // Tap on "India".
  [[EarlGrey selectElementWithMatcher:CountryEntry(@"India")]
      performAction:grey_tap()];

  // The "Done" button should not be enabled now since "State" is a required
  // field for "India".
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [SigninEarlGrey signOut];
}

// Tests that the footer text is correctly displayed when there are multiple
// required empty fields.
- (void)testFooterWithMultipleErrors {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [AutofillAppInterface saveExampleAccountProfile];
  [self openEditProfile:kProfileLabel];

  // Change text of city to empty.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(@"City")]
      performAction:grey_replaceText(@"")];

  [[EarlGrey
      selectElementWithMatcher:[self footerWithCountOfEmptyRequiredFields:1]]
      assertWithMatcher:grey_nil()];

  // Change text of state to empty.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(@"State")]
      performAction:grey_replaceText(@"")];

  [[EarlGrey
      selectElementWithMatcher:[self footerWithCountOfEmptyRequiredFields:2]]
      assertWithMatcher:grey_nil()];
  [SigninEarlGrey signOut];
}

// Tests that the local profile is migrated to account.
// TODO(crbug.com/435334012): Reenable this test.
- (void)FLAKY_testMigrateToAccount {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [AutofillAppInterface saveExampleProfile];
  [self
      openEditProfile:
          [NSString
              stringWithFormat:@"%@, %@", kProfileLabel,
                               l10n_util::GetNSString(
                                   IDS_IOS_LOCAL_ADDRESS_ACCESSIBILITY_LABEL)]];

  if ([ChromeEarlGrey isIPadIdiom]) {
    // Scroll to the bottom for ipad.
    [self scrollDownWithMatcher:grey_accessibilityID(
                                    kAutofillProfileEditTableViewId)];
  }

  [[EarlGrey selectElementWithMatcher:MigrateToAccountButton()]
      performAction:grey_tap()];
  // Wait for the snackbar to appear.
  id<GREYMatcher> snackbarMatcher = chrome_test_util::SnackbarViewMatcher();
  [ChromeEarlGrey testUIElementAppearanceWithMatcher:snackbarMatcher];
  // Tap the snackbar to make it disappear.
  [[EarlGrey selectElementWithMatcher:snackbarMatcher]
      performAction:grey_tap()];

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];
  // Open the profile view.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(kProfileLabel)]
      performAction:grey_tap()];

  id<GREYMatcher> accountProfileFooterMatcher =
      grey_text(l10n_util::GetNSStringF(
          IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT,
          u"foo1@gmail.com"));

  // Switch on edit mode to make sure the page has opened.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:accountProfileFooterMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGrey signOut];
}

// Tests that a local incomplete profile can be migrated to account after
// editing the profile.
- (void)testIncompleteProfileMigrateToAccount {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [AutofillAppInterface saveExampleProfile];

  [self
      openEditProfile:
          [NSString
              stringWithFormat:@"%@, %@", kProfileLabel,
                               l10n_util::GetNSString(
                                   IDS_IOS_LOCAL_ADDRESS_ACCESSIBILITY_LABEL)]];
  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  // Change text of city to empty.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(@"City")]
      performAction:grey_replaceText(@"")];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  if ([ChromeEarlGrey isIPadIdiom]) {
    // Scroll to the bottom for ipad.
    [self scrollDownWithMatcher:grey_accessibilityID(
                                    kAutofillProfileEditTableViewId)];
  }

  [[EarlGrey selectElementWithMatcher:MigrateToAccountButton()]
      performAction:grey_tap()];

  // Change text of city to empty.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(@"City")]
      performAction:grey_replaceText(@"New York")];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Wait for the snackbar to appear.
  id<GREYMatcher> snackbar_matcher = chrome_test_util::SnackbarViewMatcher();
  ConditionBlock wait_for_appearance = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:snackbar_matcher]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kSnackbarAppearanceTimeout, wait_for_appearance),
             @"Snackbar did not appear.");

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];
  // Open the profile view.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(kProfileLabel)]
      performAction:grey_tap()];

  id<GREYMatcher> accountProfileFooterMatcher =
      grey_text(l10n_util::GetNSStringF(
          IDS_IOS_SETTINGS_AUTOFILL_ACCOUNT_ADDRESS_FOOTER_TEXT,
          u"foo1@gmail.com"));

  // Switch on edit mode to make sure the page has opened.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:accountProfileFooterMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGrey signOut];
}

// Tests that the home/work address delete results in showing a confirmation
// sheet that contains an option to remove the profile from Chrome.
- (void)testHomeAndWorkProfileRemove {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [AutofillAppInterface saveExampleHomeAndWorkAccountProfile];

  [self openProfileListInEditMode];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   [AutofillAppInterface exampleProfileName])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:[self confirmButtonForRemoveAddress]]
      performAction:grey_tap()];
  WaitForActivityOverlayToDisappear();

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      assertWithMatcher:grey_nil()];
  // If the done button in the nav bar is enabled it is no longer in edit
  // mode.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [SigninEarlGrey signOut];
}

// Tests that the home/work address delete results in showing a confirmation
// sheet that contains an option to edit the profile in the Google Account.
- (void)testHomeAndWorkProfileDeleteOnEdit {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [AutofillAppInterface saveExampleHomeAndWorkAccountProfile];

  [self openProfileListInEditMode];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   [AutofillAppInterface exampleProfileName])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 [self editButtonInAddressDeletionConfirmationSheet]]
      performAction:grey_tap()];
  WaitForActivityOverlayToDisappear();

  // Assert that the edit page is no longer displayed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAutofillProfileEditTableViewId)]
      assertWithMatcher:grey_nil()];

  [SigninEarlGrey signOut];
}

@end
