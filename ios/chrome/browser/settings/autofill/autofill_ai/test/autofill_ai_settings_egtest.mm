// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <optional>

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/common/autofill_debug_features.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/personal_context/core/personal_context_debug_features.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/device_reauth/test/reauthentication_app_interface.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/public/autofill_ai_settings_constants.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/test/autofill_ai_settings_test_util.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
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

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::NavigationBarEditButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SettingsToolbarAddButton;

namespace {

NSString* const kOwnerName = @"autofilltestuser";
NSString* const kRedressNumber = @"1234567";
NSString* const kNewOwnerName = @"autofilltestuser2";

// Constants for testing the edit button and section deletion with redress
// entities.
NSString* const kFirstRedressName = @"Entity 1";
NSString* const kFirstRedressNumber = @"111";

// Constant for adding a vehicle entity.
NSString* const kVehicleMake = @"Audi";

// Returns the title for the Redress numbers section in Travel.
NSString* RedressNumbersSectionTitle() {
  return l10n_util::GetNSString(IDS_AUTOFILL_AI_REDRESS_NUMBER_ENTITY_NAME);
}

// Returns the title for the Vehicles section in Travel.
NSString* VehiclesSectionTitle() {
  return l10n_util::GetNSString(IDS_AUTOFILL_AI_VEHICLES_TITLE);
}

// Matcher for a table section header with `title`.
id<GREYMatcher> SectionHeaderWithTitle(NSString* title) {
  return grey_allOf(grey_accessibilityLabel(title),
                    grey_accessibilityTrait(UIAccessibilityTraitHeader), nil);
}

// Matcher for the visible table view.
id<GREYMatcher> VisibleTableView() {
  return grey_allOf(grey_kindOfClass([UITableView class]),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the toolbar's "Done" button.
id<GREYMatcher> SettingsToolbarDoneButton() {
  return grey_accessibilityID(kSettingsToolbarEditDoneButtonId);
}

}  // namespace

@interface AutofillAISettingsEGTest : ChromeTestCase
@end

@implementation AutofillAISettingsEGTest

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(
      autofill::features::kAutofillAiWithDataSchema);
  config.features_enabled.push_back(
      autofill::features::kAutofillAiReauthRequired);

  if ([self isRunningTest:@selector(testAddAndDeleteEntityUsingMenu)] ||
      [self isRunningTest:@selector(testSimultaneousRowAndSectionDeletion)]) {
    config.features_enabled.push_back(
        autofill::features::debug::kAutofillAiForceOptIn);
  }

  if ([self isRunningTest:@selector(testToggleSuggestionsFromGeminiSwitch)]) {
    config.features_enabled.push_back(
        autofill::features::kAutofillAmbientAutofill);
    config.features_enabled.push_back(
        autofill::features::kAutofillAiAvailableByDefault);
    config.features_enabled_and_params.push_back(
        {personal_context::features::debug::
             kPersonalContextForceEnablementState,
         {{"state", "2"}}});
  }

  return config;
}

- (void)setUp {
  [super setUp];
  [AutofillAppInterface
      removeEntityModifiedSince:[NSDate dateWithTimeIntervalSince1970:0]];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  if ([self isRunningTest:@selector(testToggleSuggestionsFromGeminiSwitch)]) {
    [SigninEarlGrey addFakeIdentity:fakeIdentity
                   withCapabilities:@{
                     @(kCanUseModelExecutionFeaturesName) : @YES,
                     @(kCanContextuallyUseGeminiInChromeCapabilityName) : @YES,
                   }];
  }
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
}

- (void)tearDownHelper {
  [AutofillAppInterface
      removeEntityModifiedSince:[NSDate dateWithTimeIntervalSince1970:0]];
  [ReauthenticationAppInterface mockReauthenticationModuleCanAttempt:NO];
  [ReauthenticationAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  [ReauthenticationAppInterface mockReauthenticationModuleShouldSkipReAuth:YES];
  [SigninEarlGrey signOut];
  [super tearDownHelper];
}

#pragma mark - Helpers

// Closes the settings menu from a subpage under "Autofill and passwords".
- (void)exitSettingsMenu {
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SettingsDoneButton()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Adds a vehicle entity via the UI.
- (void)addVehicleEntityViaUI {
  [[EarlGrey selectElementWithMatcher:SettingsToolbarAddButton()]
      performAction:grey_tap()];

  // Tap the "Vehicle" menu item.
  id<GREYMatcher> vehicleMenuItem =
      grey_allOf(grey_accessibilityLabel(base::SysUTF16ToNSString(
                     autofill::EntityType(autofill::EntityTypeName::kVehicle)
                         .GetNameForI18n())),
                 grey_accessibilityTrait(UIAccessibilityTraitButton),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:vehicleMenuItem]
      performAction:grey_tap()];

  [AutofillAISettingsTestUtil entityEditViewIsVisible:YES];

  // Fill "Make".
  [[EarlGrey
      selectElementWithMatcher:
          [AutofillAISettingsTestUtil
              textFieldForType:autofill::AttributeTypeName::kVehicleMake]]
      performAction:grey_replaceText(kVehicleMake)];

  // Fill "License plate".
  NSString* const kVehiclePlateNumber = @"654321";
  [[EarlGrey selectElementWithMatcher:
                 [AutofillAISettingsTestUtil
                     textFieldForType:autofill::AttributeTypeName::
                                          kVehiclePlateNumber]]
      performAction:grey_replaceText(kVehiclePlateNumber)];

  // Save the entity.
  id<GREYMatcher> saveButton = ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_SAVE_ENTITY_IN_SETTINGS_BUTTON_TEXT));
  [[EarlGrey selectElementWithMatcher:saveButton] performAction:grey_tap()];

  [AutofillAISettingsTestUtil entityEditViewIsVisible:NO];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:NavigationBarEditButton()];
}

// Opens Travel settings with initial Redress number and Vehicle test entities.
- (void)setUpTravelSettingsWithTestEntities {
  NSString* uuid = [AutofillAppInterface
      saveRedressNumberEntityWithName:kFirstRedressName
                               number:kFirstRedressNumber];
  GREYAssertNotNil(uuid, @"Failed to create redress entity.");
  GREYAssertTrue([AutofillAppInterface saveVehicleEntity],
                 @"Failed to create vehicle entity.");

  [AutofillAISettingsTestUtil openTravelSettings];

  // Verify that both Redress number and Vehicles sections are visible.
  [[[EarlGrey selectElementWithMatcher:SectionHeaderWithTitle(
                                           RedressNumbersSectionTitle())]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:VisibleTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[[EarlGrey
      selectElementWithMatcher:SectionHeaderWithTitle(VehiclesSectionTitle())]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:VisibleTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Enters edit mode, selects `rowsToDelete`, and deletes them.
- (void)deleteEntityRows:(NSArray<NSString*>*)rowsToDelete {
  // Switch to edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  for (NSString* rowLabel in rowsToDelete) {
    [[[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(rowLabel),
                                            grey_interactable(), nil)]
           usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
        onElementWithMatcher:VisibleTableView()] performAction:grey_tap()];
  }

  // Tap the "Delete" button in the bottom toolbar.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      performAction:grey_tap()];

  // Tap the confirm button in the action sheet.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
                     IDS_IOS_DELETE_ACTION_TITLE)] performAction:grey_tap()];
  WaitForActivityOverlayToDisappear();
}

// Swipes the entity with `label` to reveal the "Delete" button, taps it, and
// confirms deletion in the action sheet.
- (void)swipeToDeleteEntityWithLabel:(NSString*)label {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(label)]
      performAction:chrome_test_util::SwipeToShowDeleteButton()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(ButtonWithAccessibilityLabel(
                                              l10n_util::GetNSString(
                                                  IDS_IOS_DELETE_ACTION_TITLE)),
                                          grey_not(grey_accessibilityTrait(
                                              UIAccessibilityTraitNotEnabled)),
                                          nil)] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
                     IDS_IOS_DELETE_ACTION_TITLE)] performAction:grey_tap()];
  WaitForActivityOverlayToDisappear();
}

#pragma mark - Tests

// Tests entity editing from "Travel" in Settings.
- (void)testEntityEdit {
  NSString* uuidString =
      [AutofillAppInterface saveRedressNumberEntityWithName:kOwnerName
                                                     number:kRedressNumber];
  GREYAssertNotNil(uuidString, @"Failed to create an entity.");

  // Open "Travel" settings.
  [AutofillAISettingsTestUtil openTravelSettings];

  // Check the newly saved entity is visible.
  [AutofillAISettingsTestUtil entityWithLabel:kOwnerName isVisible:YES];

  // Tap the entity to open the entity edit view.
  [AutofillAISettingsTestUtil tapEntityWithLabel:kOwnerName];

  // Check the entity edit view is visible.
  [AutofillAISettingsTestUtil entityEditViewIsVisible:YES];

  // Start editing the entity.
  [AutofillAISettingsTestUtil startFieldEditing];

  // Tap the UITextField to edit the owner name.
  id<GREYMatcher> textFieldMatcher = [AutofillAISettingsTestUtil
      textFieldForType:autofill::AttributeTypeName::kRedressNumberName];

  [[EarlGrey selectElementWithMatcher:textFieldMatcher]
      performAction:grey_tap()];

  // Replace the owner name with a new name.
  [[EarlGrey selectElementWithMatcher:textFieldMatcher]
      performAction:grey_replaceText(kNewOwnerName)];

  // Tap the edit done button to save the change.
  [AutofillAISettingsTestUtil tapEditDoneButton];

  // Tap the back button to return to the previous screen.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  // Check the new entity is visible and the old one is not.
  [AutofillAISettingsTestUtil entityWithLabel:kNewOwnerName isVisible:YES];
  [AutofillAISettingsTestUtil entityWithLabel:kOwnerName isVisible:NO];
}

// Tests that the verification switch does not change if reauthentication fails.
- (void)testVerificationSwitchReauthFailure {
  [ReauthenticationAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kFailure];
  [ReauthenticationAppInterface mockReauthenticationModuleCanAttempt:YES];

  [AutofillAISettingsTestUtil openAutofillSettings];

  // Try to toggle the verification switch off.
  [[[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillVerificationSwitchTableViewId,
                                   /*is_toggled_on=*/YES,
                                   /*is_enabled=*/YES)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:VisibleTableView()]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Verify the switch remains toggled on.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillVerificationSwitchTableViewId,
                                   /*is_toggled_on=*/YES,
                                   /*is_enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the verification switch changes if reauthentication succeeds.
- (void)testVerificationSwitchReauthSuccess {
  [ReauthenticationAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  [ReauthenticationAppInterface mockReauthenticationModuleCanAttempt:YES];

  [AutofillAISettingsTestUtil openAutofillSettings];

  // Toggle the verification switch off.
  [[[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillVerificationSwitchTableViewId,
                                   /*is_toggled_on=*/YES,
                                   /*is_enabled=*/YES)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:VisibleTableView()]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Verify the switch is now toggled off.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kAutofillVerificationSwitchTableViewId,
                                   /*is_toggled_on=*/NO,
                                   /*is_enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that a new entity (passport) can be added, saved, and deleted using
// the add menu in Identity Docs settings.
- (void)testAddAndDeleteEntityUsingMenu {
  [AutofillAISettingsTestUtil openIdentityDocsSettings];

  // Verify the "Add" button is visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SettingsToolbarAddButton()];

  // Tap the "Add" button.
  [[EarlGrey selectElementWithMatcher:SettingsToolbarAddButton()]
      performAction:grey_tap()];

  // Tap the "Passport" menu item.
  id<GREYMatcher> passportMenuItem =
      grey_allOf(grey_accessibilityLabel(base::SysUTF16ToNSString(
                     autofill::EntityType(autofill::EntityTypeName::kPassport)
                         .GetNameForI18n())),
                 grey_accessibilityTrait(UIAccessibilityTraitButton),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:passportMenuItem]
      performAction:grey_tap()];

  [AutofillAISettingsTestUtil entityEditViewIsVisible:YES];

  // Fill "Number".
  NSString* const kPassportNumber = @"LR1234567";
  [[EarlGrey
      selectElementWithMatcher:
          [AutofillAISettingsTestUtil
              textFieldForType:autofill::AttributeTypeName::kPassportNumber]]
      performAction:grey_replaceText(kPassportNumber)];

  // Fill "Name".
  NSString* const kPassportName = @"John Doe";
  [[EarlGrey
      selectElementWithMatcher:
          [AutofillAISettingsTestUtil
              textFieldForType:autofill::AttributeTypeName::kPassportName]]
      performAction:grey_replaceText(kPassportName)];

  // Save the entity.
  id<GREYMatcher> saveButton = ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_SAVE_ENTITY_IN_SETTINGS_BUTTON_TEXT));
  [[EarlGrey selectElementWithMatcher:saveButton] performAction:grey_tap()];

  // Verify the entity appears in the list.
  [AutofillAISettingsTestUtil entityWithLabel:kPassportName isVisible:YES];

  // Swipe to delete the entity.
  [self swipeToDeleteEntityWithLabel:kPassportName];

  // Verify the entity has been deleted.
  [AutofillAISettingsTestUtil entityWithLabel:kPassportName isVisible:NO];
}

// Tests that an entity can be deleted via swipe-to-delete in Travel settings.
- (void)testSwipeToDeleteEntityInTravel {
  NSString* uuid = [AutofillAppInterface
      saveRedressNumberEntityWithName:kFirstRedressName
                               number:kFirstRedressNumber];
  GREYAssertNotNil(uuid, @"Failed to create redress entity.");

  [AutofillAISettingsTestUtil openTravelSettings];
  [AutofillAISettingsTestUtil entityWithLabel:kFirstRedressName isVisible:YES];

  [self swipeToDeleteEntityWithLabel:kFirstRedressName];

  [AutofillAISettingsTestUtil entityWithLabel:kFirstRedressName isVisible:NO];
}

// Tests that deleting the last entity in a section removes the section.
- (void)testDeleteLastEntityInSectionRemovesSection {
  [self setUpTravelSettingsWithTestEntities];

  [self deleteEntityRows:@[ kFirstRedressName ]];

  // Verify that the "Redress number" section is gone, while the "Vehicles"
  // section remains.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:SectionHeaderWithTitle(
                                                 RedressNumbersSectionTitle())];
  [[[EarlGrey
      selectElementWithMatcher:SectionHeaderWithTitle(VehiclesSectionTitle())]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:VisibleTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests mixed deletion of a section and a row.
- (void)testSimultaneousRowAndSectionDeletion {
  [self setUpTravelSettingsWithTestEntities];

  // Add a second vehicle via UI to have a multi-row section.
  [self addVehicleEntityViaUI];

  [self deleteEntityRows:@[ kFirstRedressName, kVehicleMake ]];

  // Verify that the "Redress number" section is gone, while the "Vehicles"
  // section remains.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:SectionHeaderWithTitle(
                                                 RedressNumbersSectionTitle())];
  [[[EarlGrey
      selectElementWithMatcher:SectionHeaderWithTitle(VehiclesSectionTitle())]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:VisibleTableView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that adding a new entity (Redress) enables the "Edit" button.
- (void)testEditButtonEnablesOnAddingEntity {
  [AutofillAISettingsTestUtil openTravelSettings];

  // Verify the "Edit" button is initially disabled.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      assertWithMatcher:grey_not(grey_interactable())];

  NSString* uuid = [AutofillAppInterface
      saveRedressNumberEntityWithName:kFirstRedressName
                               number:kFirstRedressNumber];
  GREYAssertNotNil(uuid, @"Failed to create redress entity.");

  // Verify the "Edit" button is now enabled.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_allOf(NavigationBarEditButton(), grey_interactable(), nil)];
}

// Tests that the editing mode is exited upon deleting the last local entity.
- (void)testAutoExitEditModeOnDeletion {
  [AutofillAISettingsTestUtil openTravelSettings];

  NSString* uuid = [AutofillAppInterface
      saveRedressNumberEntityWithName:kFirstRedressName
                               number:kFirstRedressNumber];
  GREYAssertNotNil(uuid, @"Failed to create redress entity.");

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_allOf(NavigationBarEditButton(), grey_interactable(), nil)];
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SettingsToolbarDoneButton()];

  // Delete the entity.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(kFirstRedressName)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
                     IDS_IOS_DELETE_ACTION_TITLE)] performAction:grey_tap()];
  WaitForActivityOverlayToDisappear();

  // Verify that we automatically exited edit mode.
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:SettingsToolbarDoneButton()];
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      assertWithMatcher:grey_not(grey_interactable())];
}

// Tests that the Enhanced Autofill switch can be toggled in Autofill settings.
- (void)testToggleEnhancedAutofillSwitch {
  [AutofillAISettingsTestUtil openAutofillSettings];

  id<GREYMatcher> switchMatcher =
      grey_accessibilityID(kEnhancedAutofillSwitchViewId);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:switchMatcher];

  // Verify initially OFF.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kEnhancedAutofillSwitchViewId,
                                   /*is_toggled_on=*/NO, /*is_enabled=*/YES)]
      assertWithMatcher:grey_notNil()];

  // Toggle ON.
  [[EarlGrey selectElementWithMatcher:switchMatcher]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

  // Go back to "Autofill and passwords" and re-open "Autofill settings" to
  // verify persistence.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      grey_accessibilityID(kSettingsPasswordsCellId)];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSettingsAutofillSettingsCellId),
                                          grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:VisibleTableView()] performAction:grey_tap()];

  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:switchMatcher];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kEnhancedAutofillSwitchViewId,
                                   /*is_toggled_on=*/YES, /*is_enabled=*/YES)]
      assertWithMatcher:grey_notNil()];

  // Toggle OFF.
  [[EarlGrey selectElementWithMatcher:switchMatcher]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Go back and re-open to verify it persisted OFF.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      grey_accessibilityID(kSettingsPasswordsCellId)];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSettingsAutofillSettingsCellId),
                                          grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:VisibleTableView()] performAction:grey_tap()];

  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:switchMatcher];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kEnhancedAutofillSwitchViewId,
                                   /*is_toggled_on=*/NO, /*is_enabled=*/YES)]
      assertWithMatcher:grey_notNil()];

  [self exitSettingsMenu];
}

// Tests that toggling the Suggestions from Gemini switch in the subpage updates
// the detail text on the Identity Docs settings page.
- (void)testToggleSuggestionsFromGeminiSwitch {
  [AutofillAISettingsTestUtil openIdentityDocsSettings];

  // Verify initial detail text is "On".
  id<GREYMatcher> labelMatcher = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_TITLE));
  id<GREYMatcher> valueOnMatcher =
      grey_accessibilityValue(l10n_util::GetNSString(IDS_IOS_SETTING_ON));
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_allOf(grey_accessibilityID(kSuggestionsFromGeminiTableViewId),
                     labelMatcher, valueOnMatcher, nil)];

  // Tap on the Suggestions from Gemini item to open the sub-page.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSuggestionsFromGeminiTableViewId)]
      performAction:grey_tap()];

  id<GREYMatcher> switchMatcher =
      grey_accessibilityID(kSuggestionsFromGeminiSwitchViewId);
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:switchMatcher];

  // Verify initially ON.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kSuggestionsFromGeminiSwitchViewId,
                                   /*is_toggled_on=*/YES, /*is_enabled=*/YES)]
      assertWithMatcher:grey_notNil()];

  // Toggle OFF.
  [[EarlGrey selectElementWithMatcher:switchMatcher]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Go back to Identity Docs settings.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  // Verify that the detail text is now "Off".
  id<GREYMatcher> valueOffMatcher =
      grey_accessibilityValue(l10n_util::GetNSString(IDS_IOS_SETTING_OFF));
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_allOf(grey_accessibilityID(kSuggestionsFromGeminiTableViewId),
                     labelMatcher, valueOffMatcher, nil)];

  // Tap again to enter sub-page.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSuggestionsFromGeminiTableViewId)]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:switchMatcher];

  // Toggle ON.
  [[EarlGrey selectElementWithMatcher:switchMatcher]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

  // Go back.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton(0)]
      performAction:grey_tap()];

  // Verify that the detail text is "On".
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_allOf(grey_accessibilityID(kSuggestionsFromGeminiTableViewId),
                     labelMatcher, valueOnMatcher, nil)];

  [self exitSettingsMenu];
}

@end
