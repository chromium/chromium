// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import <memory>

#import "components/translate/core/browser/translate_pref_names.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_ui_constants.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/strings/grit/ui_strings.h"

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::TabGridEditButton;
using chrome_test_util::TableViewSwitchCell;
using chrome_test_util::TurnTableViewSwitchOn;

namespace {

// Labels used to identify language entries.
NSString* const kLanguageEntryThreeLabelsTemplate = @"%@, %@, %@";
NSString* const kLanguageEntryTwoLabelsTemplate = @"%@, %@";
NSString* const kEnglishLabel = @"English";
NSString* const kTurkishLabel = @"Turkish";
NSString* const kTurkishNativeLabel = @"Türkçe";
NSString* const kAragoneseLabel = @"Aragonese";
NSString* const kNeverTranslateLabel = @"Never translate";
NSString* const kOfferToTranslateLabel = @"Offer to translate";

// Matcher for the Language Settings's main page table view.
id<GREYMatcher> LanguageSettingsTableView() {
  return grey_accessibilityID(
      kLanguageSettingsTableViewAccessibilityIdentifier);
}

// Matcher for the Language Settings's Add Language page table view.
id<GREYMatcher> AddLanguageTableView() {
  return grey_accessibilityID(kAddLanguageTableViewAccessibilityIdentifier);
}

// Matcher for the Language Settings's Language Details page table view.
id<GREYMatcher> LanguageDetailsTableView() {
  return grey_accessibilityID(kLanguageDetailsTableViewAccessibilityIdentifier);
}

// Matcher for the Language Settings's general Settings menu entry.
id<GREYMatcher> LanguageSettingsButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_LANGUAGE_SETTINGS_TITLE),
      grey_sufficientlyVisible(), nil);
}

// Matcher for the Add Language button in Language Settings's main page.
id<GREYMatcher> AddLanguageButton() {
  return grey_allOf(ButtonWithAccessibilityLabelId(
                        IDS_IOS_LANGUAGE_SETTINGS_ADD_LANGUAGE_BUTTON_TITLE),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the search bar.
id<GREYMatcher> SearchBar() {
  return grey_allOf(
      grey_accessibilityID(kAddLanguageSearchControllerAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

// Matcher for the search bar's cancel button.
id<GREYMatcher> SearchBarCancelButton() {
  return grey_allOf(ButtonWithAccessibilityLabelId(IDS_APP_CANCEL),
                    grey_kindOfClass([UIButton class]),
                    grey_ancestor(grey_kindOfClass([UISearchBar class])),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the search bar's scrim.
id<GREYMatcher> SearchBarScrim() {
  return grey_accessibilityID(kAddLanguageSearchScrimAccessibilityIdentifier);
}

// Matcher for a language entry with the given accessibility label. Matches a
// button if `tappable` is true.
id<GREYMatcher> LanguageEntry(NSString* label, BOOL tappable = YES) {
  return grey_allOf(tappable ? ButtonWithAccessibilityLabel(label)
                             : grey_accessibilityLabel(label),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the "Never Translate" button in the Language Details page.
id<GREYMatcher> NeverTranslateButton() {
  return grey_allOf(ButtonWithAccessibilityLabelId(
                        IDS_IOS_LANGUAGE_SETTINGS_NEVER_TRANSLATE_TITLE),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the "Offer to Translate" button in the Language Details page.
id<GREYMatcher> OfferToTranslateButton() {
  return grey_allOf(ButtonWithAccessibilityLabelId(
                        IDS_IOS_LANGUAGE_SETTINGS_OFFER_TO_TRANSLATE_TITLE),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for an element with or without the
// UIAccessibilityTraitSelected accessibility trait depending on `selected`.
id<GREYMatcher> ElementIsSelected(BOOL selected) {
  return selected
             ? grey_accessibilityTrait(UIAccessibilityTraitSelected)
             : grey_not(grey_accessibilityTrait(UIAccessibilityTraitSelected));
}

// Matcher for the delete button for a language entry in the Language Settings's
// main page.
id<GREYMatcher> LanguageEntryDeleteButton() {
  return grey_allOf(grey_accessibilityLabel(@"Delete"),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for the toolbar's edit button.
id<GREYMatcher> SettingToolbarEditButton() {
  return grey_accessibilityID(kSettingsToolbarEditButtonId);
}

}  // namespace

@interface LanguageSettingsTestCase : ChromeTestCase
@end

@implementation LanguageSettingsTestCase

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:translate::prefs::kOfferTranslateEnabled];
  [LanguageSettingsAppInterface removeAllLanguages];
  [LanguageSettingsAppInterface addLanguage:@"en"];
}

- (void)tearDown {
  [ChromeEarlGrey dismissSettings];

  [super tearDown];
}

#pragma mark - Test Cases

// Tests that the Language Settings pages are accessible.
- (void)testAccessibility {
  [ChromeEarlGreyUI openSettingsMenu];

  // Test accessibility on the Language Settings's main page.
  [ChromeEarlGreyUI tapSettingsMenuButton:LanguageSettingsButton()];
  [[EarlGrey selectElementWithMatcher:LanguageSettingsTableView()]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Test accessibility on the Add Language page.
  [[EarlGrey selectElementWithMatcher:AddLanguageButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:AddLanguageTableView()]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Navigate back.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  // Test accessibility on the Language Details page.
  NSString* languageEntryLabel = [NSString
      stringWithFormat:kLanguageEntryThreeLabelsTemplate, kEnglishLabel,
                       kEnglishLabel, kNeverTranslateLabel];
  [[EarlGrey selectElementWithMatcher:LanguageEntry(languageEntryLabel)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:LanguageDetailsTableView()]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that the Translate Switch enables/disables Translate and the UI gets
// updated accordingly.
- (void)testTranslateSwitch {
  [ChromeEarlGreyUI openSettingsMenu];

  // Go to the Language Settings page.
  [ChromeEarlGreyUI tapSettingsMenuButton:LanguageSettingsButton()];

  // Verify that the Translate switch is on and enabled. Toggle it off.
  [[EarlGrey
      selectElementWithMatcher:TableViewSwitchCell(
                                   kTranslateSwitchAccessibilityIdentifier, YES,
                                   YES)]
      performAction:TurnTableViewSwitchOn(NO)];

  // Verify the prefs are up-to-date.
  GREYAssertFalse([LanguageSettingsAppInterface offersTranslation],
                  @"Translate is expected to be disabled.");

  // Verify that "English (United States)" does not feature a label indicating
  // it is Translate-blocked and it is not tappable.
  NSString* languageEntryLabel =
      [NSString stringWithFormat:kLanguageEntryTwoLabelsTemplate, kEnglishLabel,
                                 kEnglishLabel];
  [[EarlGrey selectElementWithMatcher:LanguageEntry(languageEntryLabel,
                                                    /*tappable=*/NO)]
      assertWithMatcher:grey_notNil()];

  // Verify that the Translate switch is off and enabled. Toggle it on.
  [[EarlGrey
      selectElementWithMatcher:TableViewSwitchCell(
                                   kTranslateSwitchAccessibilityIdentifier, NO,
                                   YES)]
      performAction:TurnTableViewSwitchOn(YES)];

  // Verify the prefs are up-to-date.
  GREYAssertTrue([LanguageSettingsAppInterface offersTranslation],
                 @"Translate is expected to be enabled.");

  // Verify that "English (United States)" features a label indicating it is
  // Translate-blocked.
  languageEntryLabel = [NSString
      stringWithFormat:kLanguageEntryThreeLabelsTemplate, kEnglishLabel,
                       kEnglishLabel, kNeverTranslateLabel];
  [[EarlGrey selectElementWithMatcher:LanguageEntry(languageEntryLabel)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the Add Language page allows filtering languages and adding them
// to the list of accept languages.
- (void)testAddLanguage {
  [ChromeEarlGreyUI openSettingsMenu];

  // Go to the Language Settings page.
  [ChromeEarlGreyUI tapSettingsMenuButton:LanguageSettingsButton()];

  // Go to the Add Language page.
  [[EarlGrey selectElementWithMatcher:AddLanguageButton()]
      performAction:grey_tap()];

  // Verify the "Turkish" language entry is not currently visible.
  NSString* languageEntryLabel =
      [NSString stringWithFormat:kLanguageEntryTwoLabelsTemplate, kTurkishLabel,
                                 kTurkishNativeLabel];
  [[EarlGrey selectElementWithMatcher:LanguageEntry(languageEntryLabel)]
      assertWithMatcher:grey_nil()];

  // Focus the search bar.
  [[EarlGrey selectElementWithMatcher:SearchBar()] performAction:grey_tap()];

  // Verify the scrim is visible when search bar is focused but not typed in.
  [[EarlGrey selectElementWithMatcher:SearchBarScrim()]
      assertWithMatcher:grey_notNil()];

  // Verify the cancel button is visible and unfocuses search bar when tapped.
  [[EarlGrey selectElementWithMatcher:SearchBarCancelButton()]
      performAction:grey_tap()];

  // Verify languages are searchable using their name in the current locale.
  [[EarlGrey selectElementWithMatcher:SearchBar()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SearchBar()]
      performAction:grey_replaceText(kTurkishLabel)];

  // Verify that scrim is not visible anymore.
  [[EarlGrey selectElementWithMatcher:SearchBarScrim()]
      assertWithMatcher:grey_nil()];

  // Verify the "Turkish" language entry is visible.
  [[EarlGrey selectElementWithMatcher:LanguageEntry(languageEntryLabel)]
      assertWithMatcher:grey_notNil()];

  // Clear the search.
  [[EarlGrey selectElementWithMatcher:SearchBar()]
      performAction:grey_replaceText(@"")];

  // Verify the scrim is visible again.
  [[EarlGrey selectElementWithMatcher:SearchBarScrim()]
      assertWithMatcher:grey_notNil()];

  // Verify languages are searchable using their name in their native locale.
  [[EarlGrey selectElementWithMatcher:SearchBar()]
      performAction:grey_replaceText(kTurkishNativeLabel)];

  // Verify the "Turkish" language entry is visible and tap it.
  [[EarlGrey selectElementWithMatcher:LanguageEntry(languageEntryLabel)]
      performAction:grey_tap()];

  // Verify that we navigated back to the Language Settings's main page.
  [[EarlGrey selectElementWithMatcher:LanguageSettingsTableView()]
      assertWithMatcher:grey_notNil()];

  // Verify "Turkish" was added to the list of accept languages.
  languageEntryLabel = [NSString
      stringWithFormat:kLanguageEntryThreeLabelsTemplate, kTurkishLabel,
                       kTurkishNativeLabel, kNeverTranslateLabel];
  [[EarlGrey selectElementWithMatcher:LanguageEntry(languageEntryLabel)]
      assertWithMatcher:grey_notNil()];

  // Verify the prefs are up-to-date.
  GREYAssertEqualObjects([LanguageSettingsAppInterface languages], @"en,tr",
                         @"Unexpected value for accept lang pref");
}

// Tests that the Language Details page allows blocking/unblocking languages.
- (void)testLanguageDetails {
  [ChromeEarlGreyUI openSettingsMenu];

  // Add "Turkish" to the list of accept languages.
  [LanguageSettingsAppInterface addLanguage:@"tr"];
  // Verify the prefs are up-to-date.
  GREYAssertTrue([LanguageSettingsAppInterface isBlockedLanguage:@"tr"],
                 @"Turkish is expected to be Translate-blocked");

  // Go to the Language Settings page.
  [ChromeEarlGreyUI tapSettingsMenuButton:LanguageSettingsButton()];

  // Go to the "Turkish" Language Details page.
  NSString* languageEntryLabel = [NSString
      stringWithFormat:kLanguageEntryThreeLabelsTemplate, kTurkishLabel,
                       kTurkishNativeLabel, kNeverTranslateLabel];
  [[EarlGrey selectElementWithMatcher:LanguageEntry(languageEntryLabel)]
      performAction:grey_tap()];

  // Verify both options are enabled and "Never Translate" is selected.
  [[EarlGrey selectElementWithMatcher:NeverTranslateButton()]
      assertWithMatcher:grey_allOf(grey_userInteractionEnabled(),
                                   ElementIsSelected(YES), nil)];
  [[EarlGrey selectElementWithMatcher:OfferToTranslateButton()]
      assertWithMatcher:grey_allOf(grey_userInteractionEnabled(),
                                   ElementIsSelected(NO), nil)];

  // Tap the "Offer to Translate" button.
  [[EarlGrey selectElementWithMatcher:OfferToTranslateButton()]
      performAction:grey_tap()];

  // Verify that we navigated back to the Language Settings's main page.
  [[EarlGrey selectElementWithMatcher:LanguageSettingsTableView()]
      assertWithMatcher:grey_notNil()];

  // Verify that "Turkish" is unblocked.
  languageEntryLabel = [NSString
      stringWithFormat:kLanguageEntryThreeLabelsTemplate, kTurkishLabel,
                       kTurkishNativeLabel, kOfferToTranslateLabel];
  [[EarlGrey selectElementWithMatcher:LanguageEntry(languageEntryLabel)]
      assertWithMatcher:grey_notNil()];

  // Verify the prefs are up-to-date.
  GREYAssertFalse([LanguageSettingsAppInterface isBlockedLanguage:@"tr"],
                  @"Turkish should not be Translate-blocked");

  // Go to the "Turkish" Language Details page.
  [[EarlGrey selectElementWithMatcher:LanguageEntry(languageEntryLabel)]
      performAction:grey_tap()];

  // Verify both options are enabled and "Offer to Translate" is selected.
  [[EarlGrey selectElementWithMatcher:NeverTranslateButton()]
      assertWithMatcher:grey_allOf(grey_userInteractionEnabled(),
                                   ElementIsSelected(NO), nil)];
  [[EarlGrey selectElementWithMatcher:OfferToTranslateButton()]
      assertWithMatcher:grey_allOf(grey_userInteractionEnabled(),
                                   ElementIsSelected(YES), nil)];

  // Tap the "Never Translate" button.
  [[EarlGrey selectElementWithMatcher:NeverTranslateButton()]
      performAction:grey_tap()];

  // Verify that we navigated back to the Language Settings's main page.
  [[EarlGrey selectElementWithMatcher:LanguageSettingsTableView()]
      assertWithMatcher:grey_notNil()];

  // Verify "Turkish" is Translate-blocked.
  languageEntryLabel = [NSString
      stringWithFormat:kLanguageEntryThreeLabelsTemplate, kTurkishLabel,
                       kTurkishNativeLabel, kNeverTranslateLabel];
  [[EarlGrey selectElementWithMatcher:LanguageEntry(languageEntryLabel)]
      assertWithMatcher:grey_notNil()];

  // Verify the prefs are up-to-date.
  GREYAssertTrue([LanguageSettingsAppInterface isBlockedLanguage:@"tr"],
                 @"Turkish is expected to be Translate-blocked");
}

// Tests that the target language cannot be unblocked.
- (void)testUnblockTargetLanguage {
  [ChromeEarlGreyUI openSettingsMenu];

  // Add "Turkish" to the list of accept languages.
  [LanguageSettingsAppInterface addLanguage:@"tr"];
  // Verify the prefs are up-to-date.
  GREYAssertTrue([LanguageSettingsAppInterface isBlockedLanguage:@"tr"],
                 @"Turkish is expected to be Translate-blocked");

  // Make "Turkish" the target language.
  [LanguageSettingsAppInterface setRecentTargetLanguage:@"tr"];

  // Go to the Language Settings page.
  [ChromeEarlGreyUI tapSettingsMenuButton:LanguageSettingsButton()];

  // Go to the "Turkish" Language Details page.
  NSString* languageEntryLabel = [NSString
      stringWithFormat:kLanguageEntryThreeLabelsTemplate, kTurkishLabel,
                       kTurkishNativeLabel, kNeverTranslateLabel];
  [[EarlGrey selectElementWithMatcher:LanguageEntry(languageEntryLabel)]
      performAction:grey_tap()];

  // Verify the "Never Translate" option is enabled and selected while
  // "Offer to Translate" is disabled and unselected.
  [[EarlGrey selectElementWithMatcher:NeverTranslateButton()]
      assertWithMatcher:grey_allOf(grey_userInteractionEnabled(),
                                   ElementIsSelected(YES), nil)];
  [[EarlGrey selectElementWithMatcher:OfferToTranslateButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_userInteractionEnabled()),
                                   ElementIsSelected(NO), nil)];
}

// Tests that the last Translate-blocked language cannot be unblocked.
- (void)testUnblockLastBlockedLanguage {
  [ChromeEarlGreyUI openSettingsMenu];

  // Make sure "Turkish" is the target language and not "en".
  [LanguageSettingsAppInterface setRecentTargetLanguage:@"tr"];

  // Go to the Language Settings page.
  [ChromeEarlGreyUI tapSettingsMenuButton:LanguageSettingsButton()];

  // Go to the "Turkish" Language Details page.
  NSString* languageEntryLabel = [NSString
      stringWithFormat:kLanguageEntryThreeLabelsTemplate, kEnglishLabel,
                       kEnglishLabel, kNeverTranslateLabel];
  [[EarlGrey selectElementWithMatcher:LanguageEntry(languageEntryLabel)]
      performAction:grey_tap()];

  // Verify the "Never Translate" option is enabled and selected while
  // "Offer to Translate" is disabled and unselected.
  [[EarlGrey selectElementWithMatcher:NeverTranslateButton()]
      assertWithMatcher:grey_allOf(grey_userInteractionEnabled(),
                                   ElementIsSelected(YES), nil)];
  [[EarlGrey selectElementWithMatcher:OfferToTranslateButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_userInteractionEnabled()),
                                   ElementIsSelected(NO), nil)];
}

// Tests that an unsupported language cannot be unblocked.
- (void)testUnblockUnsupportedLanguage {
  [ChromeEarlGreyUI openSettingsMenu];

  // Add "Aragonese" to the list of accept languages.
  [LanguageSettingsAppInterface addLanguage:@"tr"];
  [LanguageSettingsAppInterface addLanguage:@"an"];
  // Verify the prefs are up-to-date.
  GREYAssertTrue([LanguageSettingsAppInterface isBlockedLanguage:@"an"],
                 @"Aragonese is expected to be Translate-blocked");

  // Go to the Language Settings page.
  [ChromeEarlGreyUI tapSettingsMenuButton:LanguageSettingsButton()];

  // Go to the "Aragonese" Language Details page.
  NSString* languageEntryLabel = [NSString
      stringWithFormat:kLanguageEntryThreeLabelsTemplate, kAragoneseLabel,
                       kAragoneseLabel, kNeverTranslateLabel];
  [[EarlGrey selectElementWithMatcher:LanguageEntry(languageEntryLabel)]
      performAction:grey_tap()];

  // Verify the "Never Translate" option is enabled and selected while
  // "Offer to Translate" is disabled and unselected.
  [[EarlGrey selectElementWithMatcher:NeverTranslateButton()]
      assertWithMatcher:grey_allOf(grey_userInteractionEnabled(),
                                   ElementIsSelected(YES), nil)];
  [[EarlGrey selectElementWithMatcher:OfferToTranslateButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_userInteractionEnabled()),
                                   ElementIsSelected(NO), nil)];
}

// Tests that the Add Language button as well as the Translate switch are
// disabled in edit mode.
- (void)testEditMode {
  [ChromeEarlGreyUI openSettingsMenu];

  // Go to the Language Settings page.
  [ChromeEarlGreyUI tapSettingsMenuButton:LanguageSettingsButton()];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:SettingToolbarEditButton()]
      performAction:grey_tap()];

  // Verify that the Add Language button is disabled.
  [[EarlGrey selectElementWithMatcher:AddLanguageButton()]
      assertWithMatcher:grey_not(grey_userInteractionEnabled())];

  // Verify that the Translate switch is on and disabled.
  [[EarlGrey
      selectElementWithMatcher:TableViewSwitchCell(
                                   kTranslateSwitchAccessibilityIdentifier, YES,
                                   NO)] assertWithMatcher:grey_notNil()];
}

// Tests that languages, except the last one, can be deleted from the list of
// accept languages.
- (void)testDeleteLanguage {
  [ChromeEarlGreyUI openSettingsMenu];

  // Add "Turkish" to the list of accept languages.
  [LanguageSettingsAppInterface addLanguage:@"tr"];
  // Verify the prefs are up-to-date.
  GREYAssertTrue([LanguageSettingsAppInterface isBlockedLanguage:@"tr"],
                 @"Turkish is expected to be Translate-blocked");

  // Go to the Language Settings page.
  [ChromeEarlGreyUI tapSettingsMenuButton:LanguageSettingsButton()];

  // Swipe left on the "English" language entry.
  NSString* englishLanguageEntryLabel = [NSString
      stringWithFormat:kLanguageEntryThreeLabelsTemplate, kEnglishLabel,
                       kEnglishLabel, kNeverTranslateLabel];

  // swipeAction uses start point to stay small so that it works across
  // devices on iOS13.
  id swipeAction =
      grey_swipeFastInDirectionWithStartPoint(kGREYDirectionLeft, 0.15, 0.15);

  [[EarlGrey selectElementWithMatcher:LanguageEntry(englishLanguageEntryLabel)]
      performAction:swipeAction];

  // Verify that a delete button is visible.
  [[EarlGrey selectElementWithMatcher:LanguageEntryDeleteButton()]
      assertWithMatcher:grey_notNil()];

  // Swipe left on the "Turkish" language entry.
  NSString* turkishLanguageEntryLabel = [NSString
      stringWithFormat:kLanguageEntryThreeLabelsTemplate, kTurkishLabel,
                       kTurkishNativeLabel, kNeverTranslateLabel];
  [[EarlGrey selectElementWithMatcher:LanguageEntry(turkishLanguageEntryLabel)]
      performAction:swipeAction];

  // Verify that a delete button is visible and tap it.
  [[EarlGrey selectElementWithMatcher:LanguageEntryDeleteButton()]
      performAction:grey_tap()];

  // Verify that the "Turkish" language entry does not exist anymore.
  [[EarlGrey selectElementWithMatcher:LanguageEntry(turkishLanguageEntryLabel)]
      assertWithMatcher:grey_nil()];

  // Verify the prefs are up-to-date.
  GREYAssertEqualObjects([LanguageSettingsAppInterface languages], @"en",
                         @"Unexpected value for accept lang pref");

  // Swipe left on the "English" language entry.
  [[EarlGrey selectElementWithMatcher:LanguageEntry(englishLanguageEntryLabel)]
      performAction:swipeAction];

  // Verify that a delete button is not visible.
  [[EarlGrey selectElementWithMatcher:LanguageEntryDeleteButton()]
      assertWithMatcher:grey_nil()];
}

@end
