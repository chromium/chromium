// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/web_view_interaction_test_util.h"
#include "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;

namespace {

// Expectation of how the saved Autofill profile looks like, a map from cell
// name IDs to expected contents.
struct DisplayStringIDToExpectedResult {
  int display_string_id;
  NSString* expected_result;
};

const DisplayStringIDToExpectedResult kExpectedFields[] = {
    {IDS_IOS_AUTOFILL_FULLNAME, @"John H. Doe"},
    {IDS_IOS_AUTOFILL_COMPANY_NAME, @"Underworld"},
    {IDS_IOS_AUTOFILL_ADDRESS1, @"666 Erebus St."},
    {IDS_IOS_AUTOFILL_ADDRESS2, @"Apt 8"},
    {IDS_IOS_AUTOFILL_CITY, @"Elysium"},
    {IDS_IOS_AUTOFILL_STATE, @"CA"},
    {IDS_IOS_AUTOFILL_ZIP, @"91111"},
    {IDS_IOS_AUTOFILL_PHONE, @"16502111111"},
    {IDS_IOS_AUTOFILL_EMAIL, @"johndoe@hades.com"}};

NSString* const kProfileLabel = @"John H. Doe, 666 Erebus St.";

// Expectation of how user-typed country names should be canonicalized.
struct UserTypedCountryExpectedResultPair {
  NSString* user_typed_country;
  NSString* expected_result;
};

const UserTypedCountryExpectedResultPair kCountryTests[] = {
    {@"Brasil", @"Brazil"},
    {@"China", @"China"},
    {@"DEUTSCHLAND", @"Germany"},
    {@"GREAT BRITAIN", @"United Kingdom"},
    {@"IN", @"India"},
    {@"JaPaN", @"Japan"},
    {@"JP", @"Japan"},
    {@"Nigeria", @"Nigeria"},
    {@"TW", @"Taiwan"},
    {@"U.S.A.", @"United States"},
    {@"UK", @"United Kingdom"},
    {@"USA", @"United States"},
    {@"Nonexistia", @""},
};

// Given a resource ID of a category of an Autofill profile, it returns a
// NSString consisting of the resource string concatenated with "_textField".
// This is the a11y ID of the text field corresponding to the category in the
// edit dialog of the Autofill profile.
NSString* GetTextFieldForID(int categoryId) {
  return [NSString
      stringWithFormat:@"%@_textField", l10n_util::GetNSString(categoryId)];
}

}  // namespace

// Various tests for the Autofill profiles section of the settings.
@interface AutofillProfileSettingsTestCase : ChromeTestCase
@end

@implementation AutofillProfileSettingsTestCase {
  // The PersonalDataManager instance for the current browser state.
  autofill::PersonalDataManager* _personalDataManager;
}

- (void)setUp {
  [super setUp];

  _personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  _personalDataManager->SetSyncingForTest(true);
}

- (void)tearDown {
  // Clear existing profiles.
  for (const auto* profile : _personalDataManager->GetProfiles()) {
    _personalDataManager->RemoveByGUID(profile->guid());
  }

  [super tearDown];
}

- (autofill::AutofillProfile)addAutofillProfile {
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();
  size_t profileCount = _personalDataManager->GetProfiles().size();
  _personalDataManager->AddProfile(profile);
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForActionTimeout,
                 ^bool() {
                   return profileCount <
                          _personalDataManager->GetProfiles().size();
                 }),
             @"Failed to add profile.");
  return profile;
}

// Helper to open the settings page for Autofill profiles.
- (void)openAutofillProfilesSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  id<GREYMatcher> addressesButton =
      ButtonWithAccessibilityLabelId(IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE);
  [[EarlGrey selectElementWithMatcher:addressesButton]
      performAction:grey_tap()];
}

// Helper to open the settings page for the Autofill profile with |label|.
- (void)openEditProfile:(NSString*)label {
  [self openAutofillProfilesSettings];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(label)]
      performAction:grey_tap()];
}

// Close the settings.
- (void)exitSettingsMenu {
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  // Wait for UI components to finish loading.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Test that the page for viewing Autofill profile details is as expected.
- (void)testAutofillProfileViewPage {
  autofill::AutofillProfile profile = [self addAutofillProfile];
  [self openEditProfile:kProfileLabel];

  // Check that all fields and values match the expectations.
  for (const DisplayStringIDToExpectedResult& expectation : kExpectedFields) {
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityLabel([NSString
                       stringWithFormat:@"%@, %@",
                                        l10n_util::GetNSString(
                                            expectation.display_string_id),
                                        expectation.expected_result])]
        assertWithMatcher:grey_notNil()];
  }

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Test that editing country names is followed by validating the value and
// replacing it with a canonical one.
- (void)testAutofillProfileEditing {
  autofill::AutofillProfile profile = [self addAutofillProfile];
  [self openEditProfile:kProfileLabel];

  // Keep editing the Country field and verify that validation works.
  for (const UserTypedCountryExpectedResultPair& expectation : kCountryTests) {
    // Switch on edit mode.
    [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                            IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
        performAction:grey_tap()];

    // Replace the text field with the user-version of the country.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(GetTextFieldForID(
                                            IDS_IOS_AUTOFILL_COUNTRY))]
        performAction:grey_replaceText(expectation.user_typed_country)];

    // Switch off edit mode.
    [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
        performAction:grey_tap()];

    // Verify that the country value was changed to canonical.
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityLabel(
                       [NSString stringWithFormat:@"%@, %@",
                                                  l10n_util::GetNSString(
                                                      IDS_IOS_AUTOFILL_COUNTRY),
                                                  expectation.expected_result])]
        assertWithMatcher:grey_notNil()];
  }

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Test that the page for viewing Autofill profile details is accessible.
- (void)testAccessibilityOnAutofillProfileViewPage {
  autofill::AutofillProfile profile = [self addAutofillProfile];
  [self openEditProfile:kProfileLabel];

  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Test that the page for editing Autofill profile details is accessible.
- (void)testAccessibilityOnAutofillProfileEditPage {
  autofill::AutofillProfile profile = [self addAutofillProfile];
  [self openEditProfile:kProfileLabel];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Checks that the Autofill profiles list view is in edit mode and the Autofill
// profiles switch is disabled.
- (void)testListViewEditMode {
  autofill::AutofillProfile profile = [self addAutofillProfile];
  [self openAutofillProfilesSettings];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
      performAction:grey_tap()];

  // Check the Autofill profile switch is disabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::LegacySettingsSwitchCell(
                                   @"addressItem_switch", YES, NO)]
      assertWithMatcher:grey_notNil()];

  [self exitSettingsMenu];
}

// Checks that the Autofill profile switch can be toggled on/off and the list of
// Autofill profiles is not affected by it.
- (void)testToggleAutofillProfileSwitch {
  autofill::AutofillProfile profile = [self addAutofillProfile];
  [self openAutofillProfilesSettings];

  // Toggle the Autofill profiles switch off.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::LegacySettingsSwitchCell(
                                   @"addressItem_switch", YES, YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];

  // Expect Autofill profiles to remain visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(kProfileLabel)]
      assertWithMatcher:grey_notNil()];

  // Toggle the Autofill profiles switch back on.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::LegacySettingsSwitchCell(
                                   @"addressItem_switch", NO, YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Expect Autofill profiles to remain visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(kProfileLabel)]
      assertWithMatcher:grey_notNil()];

  [self exitSettingsMenu];
}

@end
