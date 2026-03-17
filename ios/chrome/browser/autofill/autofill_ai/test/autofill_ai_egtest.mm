// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

NSString* const kOwnerName = @"autofilltestuser";
NSString* const kRedressNumber = @"1234567";

// Save button on the infobar.
id<GREYMatcher> infoBarSaveButton() {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL));
}

// UITableView to show the new entity and the old entity if there is one.
id<GREYMatcher> entitiesView() {
  return grey_accessibilityID(kAutofillAISaveEntityTableViewId);
}

// Save button on the save entity view controller.
id<GREYMatcher> saveEntityButton() {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL));
}

// Taps the infobar save button.
void TapInfoBarSaveButton() {
  [[EarlGrey selectElementWithMatcher:infoBarSaveButton()]
      performAction:grey_tap()];
}

// Taps the save entity button.
void TapSaveEntityButton() {
  [[EarlGrey selectElementWithMatcher:saveEntityButton()]
      performAction:grey_tap()];
}

// Verifies if the view to show the entity is visible.
void VerifyEntityVisibility(bool is_visible) {
  [[EarlGrey selectElementWithMatcher:entitiesView()]
      assertWithMatcher:is_visible ? grey_sufficientlyVisible() : grey_nil()];
}

// Label within a table view cell in "Addresses and more" settings.
id<GREYMatcher> GetMatcherForLabel(NSString* label) {
  return grey_allOf(
      grey_accessibilityLabel(label),
      grey_ancestor(grey_accessibilityID(kAutofillProfileTableViewID)),
      grey_interactable(), nil);
}

// Verifies if a cell with the given label is visible.
void VerifyCellVisibility(NSString* entity_label, bool is_visible) {
  [[EarlGrey selectElementWithMatcher:GetMatcherForLabel(entity_label)]
      assertWithMatcher:is_visible ? grey_sufficientlyVisible()
                                   : grey_notVisible()];
}

// Verifies the flow of saving a new entity from the infobar banner.
void VerifySaveNewEntityFlow() {
  // Tap the infobar save button.
  TapInfoBarSaveButton();

  // Verify the save entity UI is shown.
  VerifyEntityVisibility(true);

  // Tap the save entity button.
  TapSaveEntityButton();

  // Verify the save entity UI is gone.
  VerifyEntityVisibility(false);
}

}  // namespace

@interface AutofillAIEGTest : ChromeTestCase
@end

@implementation AutofillAIEGTest

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      autofill::features::kAutofillAiWithDataSchema);
  config.features_enabled.push_back(
      autofill::features::kAutofillAiCreateEntityDataManager);
  config.features_enabled.push_back(
      autofill::features::kAutofillAiReauthRequired);
  return config;
}

- (void)setUp {
  [super setUp];
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
}

// Tests the infobar banner shows and can be tapped. Once tapped, the detailed
// save entity UI is shown.
- (void)testSaveEntityUI {
  // Simulate a trigger for the infobar.
  [AutofillAppInterface showAutofillAiSaveEntityBubble];

  // Verify the infobar banner shows and can be tapped. Once tapped, the
  // detailed save entity UI is shown, and save button can be tapped.
  VerifySaveNewEntityFlow();
}

// Tests entity deletion from "Addresses and more" in Settings.
- (void)testEntityDeletion {
  GREYAssertTrue(
      [AutofillAppInterface saveRedressNumberEntityWithName:kOwnerName
                                                     number:kRedressNumber],
      @"Failed to create an entity.");
  // Open "Addresses and more" from settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::AddressesAndMoreButton()];

  // Verify the entity just saved is visible in settings.
  VerifyCellVisibility(kOwnerName, true);

  // Tap the Toolbar Edit button to enter edit mode.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsToolbarEditButton()]
      performAction:grey_tap()];

  // Select the cell containing the entity.
  [[EarlGrey selectElementWithMatcher:GetMatcherForLabel(kOwnerName)]
      performAction:grey_tap()];

  // Tap the delete button in the bottom toolbar.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      performAction:grey_tap()];

  // Tap the delete action sheet button to confirm deletion.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
                     IDS_IOS_DELETE_ACTION_TITLE)] performAction:grey_tap()];

  // Ensure it's deleted.
  VerifyCellVisibility(kOwnerName, false);
}

@end
