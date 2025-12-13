// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/ios/common/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/bottom_sheet_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_traits_overrider.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SettingsProfileMatcher;
using chrome_test_util::SettingsToolbarAddButton;

namespace {

NSString* const kStreetAddressLabel =
    base::SysUTF8ToNSString(autofill::FieldTypeToDeveloperRepresentationString(
        autofill::ADDRESS_HOME_STREET_ADDRESS));

NSString* const kCityLabel =
    base::SysUTF8ToNSString(autofill::FieldTypeToDeveloperRepresentationString(
        autofill::ADDRESS_HOME_CITY));

NSString* const kStateLabel =
    base::SysUTF8ToNSString(autofill::FieldTypeToDeveloperRepresentationString(
        autofill::ADDRESS_HOME_STATE));

NSString* const kZipLabel =
    base::SysUTF8ToNSString(autofill::FieldTypeToDeveloperRepresentationString(
        autofill::ADDRESS_HOME_ZIP));

NSString* const kNameLabel = @"Name";

NSString* const kCompanyNameLabel = @"Organization";

// Matcher for the "Save Address" button.
id<GREYMatcher> SaveAddressButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL);
}

// Returns a matcher for a UITextField with an accessibility ID derived from the
// provided `text_field_label`.
id<GREYMatcher> TextFieldWithLabel(NSString* text_field_label) {
  return grey_allOf(grey_accessibilityID([text_field_label
                        stringByAppendingString:@"_textField"]),
                    grey_kindOfClass([UITextField class]), nil);
}

// Matcher for the "add address" bottom sheet.
id<GREYMatcher> EditProfileBottomSheet() {
  return grey_accessibilityID(kEditProfileBottomSheetViewIdentfier);
}

// Helper to open the address settings page.
void OpenAddressSettings() {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::AddressesAndMoreButton()];
}

// Gets the top presented view controller, in this case the bottom sheet view
// controller.
UIViewController* TopPresentedViewController() {
  UIViewController* top_controller =
      chrome_test_util::GetAnyKeyWindow().rootViewController;
  for (UIViewController* controller = [top_controller presentedViewController];
       controller && ![controller isBeingDismissed];
       controller = [controller presentedViewController]) {
    top_controller = controller;
  }
  return top_controller;
}

}  // namespace

@interface AutofillAddAddressManuallyTestCase : ChromeTestCase
@end

@implementation AutofillAddAddressManuallyTestCase

- (void)setUp {
  [super setUp];
  [AutofillAppInterface clearProfilesStore];
}

- (void)tearDownHelper {
  [AutofillAppInterface clearProfilesStore];
  [super tearDownHelper];
}

#pragma mark - Helpers

// Closes the settings.
- (void)exitSettingsMenu {
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  // Wait for UI components to finish loading.
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Populates required address fields.
- (void)fillRequiredFields {
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(kStreetAddressLabel)]
      performAction:grey_replaceText(@"Rue St Catherine")];

  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(kCityLabel)]
      performAction:grey_replaceText(@"Montreal")];

  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(kStateLabel)]
      performAction:grey_replaceText(@"Quebec")];

  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(kZipLabel)]
      performAction:grey_replaceText(@"H3H 1H1")];
}

// Helper to open the "add address" bottom sheet.
- (void)openAddAddressView:(BOOL)signIn {
  if (signIn) {
    [SigninEarlGreyUI
        signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
    [ChromeEarlGrey
        waitForSyncTransportStateActiveWithTimeout:base::Seconds(10)];
  }

  OpenAddressSettings();

  // Tap the "Add" button.
  [[EarlGrey selectElementWithMatcher:SettingsToolbarAddButton()]
      performAction:grey_tap()];

  // Verify the "add address" bottom sheet is visible.
  [[EarlGrey selectElementWithMatcher:EditProfileBottomSheet()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - Tests

// Tests adding a local address manually through settings.
- (void)testAddLocalAddressManually {
  [self openAddAddressView:NO];

  // Fill the required fields.
  [self fillRequiredFields];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:SaveAddressButton()]
      performAction:grey_tap()];

  // Ensure profile is saved.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  // Confirm saved profile is a local profile.
  GREYAssertFalse([AutofillAppInterface isAccountProfileAtIndex:0],
                  @"Profile should have been saved locally.");
}

// Tests adding an account address manually through settings.
- (void)testAddAccountAddressManually {
  // The user needs to be signed in for the address to be saved to the account.
  [self openAddAddressView:YES];

  // Fill the required fields.
  [self fillRequiredFields];

  // Scroll to bottom, this is needed in some cases because the "Save" button is
  // not always visible.
  [[EarlGrey selectElementWithMatcher:EditProfileBottomSheet()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:SaveAddressButton()]
      performAction:grey_tap()];

  // Ensure profile is saved.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  // Confirm saved profile is an account profile.
  GREYAssertTrue([AutofillAppInterface isAccountProfileAtIndex:0],
                 @"Profile should have been saved to account.");

  // Exit settings.
  [self exitSettingsMenu];

  // Sign out.
  [SigninEarlGreyUI signOut];
}

// Tests that tapping the `Cancel` button triggers the dismissal of the "add
// address" bottom sheet.
- (void)testCancelButton {
  [self openAddAddressView:NO];

  // Tap "Cancel".
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(
                                       kEditProfileBottomSheetCancelButton),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];

  // Verify the address settings opened.
  [[EarlGrey selectElementWithMatcher:SettingsProfileMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the 'Save' button enabled state when manually adding an address to the
// account.
- (void)testButtonEnabledStateAtStartForAccountAddress {
  // The user needs to be signed in for the address to be saved to the account.
  [self openAddAddressView:YES];

  // Scroll down to show the 'Save' button.
  [[EarlGrey selectElementWithMatcher:EditProfileBottomSheet()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Ensure the 'Save' button is initially disabled for an account address.
  [[EarlGrey selectElementWithMatcher:SaveAddressButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Sign out.
  [SigninEarlGrey signOut];
}

// Tests the 'Save' button enabled state when manually adding a local address.
- (void)testButtonEnabledStateAtStartForLocalAddress {
  [self openAddAddressView:NO];

  // Ensure the 'Save' button is initially enabled for a local address (user is
  // signed out).
  [[EarlGrey selectElementWithMatcher:SaveAddressButton()]
      assertWithMatcher:grey_enabled()];
}

// Tests the 'Save' button's enabled state when manually adding an address to
// the account, validating it against required field population.
- (void)testButtonEnabledStateForAccountAddress {
  // The user needs to be signed in for the address to be saved to the account.
  [self openAddAddressView:YES];

  // Fill the required fields.
  [self fillRequiredFields];

  // Scroll down to show the 'Save' button.
  [[EarlGrey selectElementWithMatcher:EditProfileBottomSheet()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Ensure the 'Save' button's status changes to enabled.
  [[EarlGrey selectElementWithMatcher:SaveAddressButton()]
      assertWithMatcher:grey_enabled()];

  // Remove one of the required fields
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(kStreetAddressLabel)]
      performAction:grey_replaceText(@"")];

  // Ensure the 'Save' button is disabled.
  [[EarlGrey selectElementWithMatcher:SaveAddressButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Fill the required field.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(kStreetAddressLabel)]
      performAction:grey_replaceText(@"Rue St Catherine")];

  // Ensure the 'Save' button is enabled.
  [[EarlGrey selectElementWithMatcher:SaveAddressButton()]
      assertWithMatcher:grey_enabled()];

  // Sign out.
  [SigninEarlGrey signOut];
}

// Tests that the correct error message is displayed based on the completeness
// of required address fields during manual address addition.
- (void)testErrorMessageForAccountAddress {
  // The user needs to be signed in for the address to be saved to the account.
  [self openAddAddressView:YES];

  // Fill the required fields.
  [self fillRequiredFields];

  // Assert that no error message is displayed at start.
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ContainsPartialText(l10n_util::GetPluralNSStringF(
              IDS_IOS_SETTINGS_EDIT_AUTOFILL_ADDRESS_REQUIREMENT_ERROR, 1))]
      assertWithMatcher:grey_nil()];

  // Remove one of the required fields.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(kStreetAddressLabel)]
      performAction:grey_replaceText(@"")];

  // Assert that an error message is displayed when one required field is
  // missing.
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ContainsPartialText(l10n_util::GetPluralNSStringF(
              IDS_IOS_SETTINGS_EDIT_AUTOFILL_ADDRESS_REQUIREMENT_ERROR, 1))]
      assertWithMatcher:grey_notNil()];

  // Remove another required field, leaving two required fields empty.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(kCityLabel)]
      performAction:grey_replaceText(@"")];

  // Assert that the error message reflects multiple missing required fields.
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ContainsPartialText(l10n_util::GetPluralNSStringF(
              IDS_IOS_SETTINGS_EDIT_AUTOFILL_ADDRESS_REQUIREMENT_ERROR, 2))]
      assertWithMatcher:grey_notNil()];

  // Fill one of the required fields to leave only one field empty.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(kStreetAddressLabel)]
      performAction:grey_replaceText(@"Rue St Catherine")];

  // Assert that an error message is displayed when one required field is
  // missing.
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ContainsPartialText(l10n_util::GetPluralNSStringF(
              IDS_IOS_SETTINGS_EDIT_AUTOFILL_ADDRESS_REQUIREMENT_ERROR, 1))]
      assertWithMatcher:grey_notNil()];

  // Fill the last required field, completing all required fields.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(kCityLabel)]
      performAction:grey_replaceText(@"Montreal")];

  // Assert that the error message disappears when all required fields are
  // filled.
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ContainsPartialText(l10n_util::GetPluralNSStringF(
              IDS_IOS_SETTINGS_EDIT_AUTOFILL_ADDRESS_REQUIREMENT_ERROR, 1))]
      assertWithMatcher:grey_nil()];

  // Sign out.
  [SigninEarlGrey signOut];
}

// Tests adding a local address manually through settings, filling only
// non-required fields.
- (void)testAddLocalAddressManuallyWithOptionalFields {
  [self openAddAddressView:NO];

  // Fill the non-required fields.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kNameLabel)]
      performAction:grey_replaceText(@"Custom Name")];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kCompanyNameLabel)]
      performAction:grey_replaceText(@"Custom Company Name")];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:SaveAddressButton()]
      performAction:grey_tap()];

  // Ensure profile is saved.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  // Confirm saved profile is a local profile.
  GREYAssertFalse([AutofillAppInterface isAccountProfileAtIndex:0],
                  @"Profile should have been saved locally.");
}

// Tests adding a local address manually through settings, filling some of the
// required fields.
- (void)testAddLocalAddressWithSomeRequiredFields {
  [self openAddAddressView:NO];

  // Fill a required field.
  [[EarlGrey selectElementWithMatcher:TextFieldWithLabel(kCityLabel)]
      performAction:grey_replaceText(@"Montreal")];

  // Save the profile.
  [[EarlGrey selectElementWithMatcher:SaveAddressButton()]
      performAction:grey_tap()];

  // Ensure profile is saved.
  GREYAssertEqual(1U, [AutofillAppInterface profilesCount],
                  @"Profile should have been saved.");

  // Confirm saved profile is a local profile.
  GREYAssertFalse([AutofillAppInterface isAccountProfileAtIndex:0],
                  @"Profile should have been saved locally.");
}

// Tests that the 'Save' button enabled state on iPads is being correctly
// updated, even when the button is not visible in the view. See
// crbug.com/410609782.
// TODO(crbug.com/411171102): Improve EGTest to correctly handle UI layout
// variations on different screen sizes.
- (void)testButtonEnabledStateLargeTextForAccountAddress {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test is not applicable for iPhone.");
  }

  if (@available(iOS 17.0, *)) {
    [self openAddAddressView:YES];

    // Change trait collection to use extra large content size so that the
    // 'Save' button becomes hidden.
    ScopedTraitOverrider overrider(TopPresentedViewController());
    overrider.SetContentSizeCategory(UIContentSizeCategoryExtraLarge);
    [ChromeEarlGreyUI waitForAppToIdle];

    // Fill the required fields.
    [self fillRequiredFields];

    // Scroll down to show the 'Save' button.
    [[EarlGrey selectElementWithMatcher:EditProfileBottomSheet()]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

    // Ensure the 'Save' button's status changed to enabled.
    [[EarlGrey selectElementWithMatcher:SaveAddressButton()]
        assertWithMatcher:grey_enabled()];
  }
}

@end
