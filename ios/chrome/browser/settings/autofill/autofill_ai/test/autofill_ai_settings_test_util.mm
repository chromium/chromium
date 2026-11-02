// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/test/autofill_ai_settings_test_util.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/public/autofill_ai_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Label within a table view cell in settings.
id<GREYMatcher> GetMatcherForLabel(NSString* label) {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_ancestor(grey_kindOfClass([UITableView class])),
                    grey_interactable(), nil);
}

// Opens the Autofill and Passwords subpage identified by `cell_id` and waits
// for `subpage_matcher` to become visible.
void OpenAutofillAndPasswordsSubpage(NSString* cell_id,
                                     id<GREYMatcher> subpage_matcher) {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:grey_accessibilityID(
                                kSettingsAutofillAndPasswordsCellId)];
  // Wait for the "Autofill and passwords" page to be pushed before scrolling.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      grey_accessibilityID(kSettingsPasswordsCellId)];
  id<GREYMatcher> cell_matcher =
      grey_allOf(grey_accessibilityID(cell_id), grey_interactable(), nil);
  [[[EarlGrey selectElementWithMatcher:cell_matcher]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:grey_allOf(grey_kindOfClass([UITableView class]),
                                      grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:subpage_matcher];
}

}  // namespace

@implementation AutofillAISettingsTestUtil

+ (void)openIdentityDocsSettings {
  OpenAutofillAndPasswordsSubpage(
      kSettingsIdentityDocsCellId,
      grey_allOf(grey_text(l10n_util::GetNSString(
                     IDS_AUTOFILL_IDENTITY_DOCS_OPT_IN_TOGGLE_LABEL)),
                 grey_kindOfClass([UILabel class]), nil));
}

+ (void)openTravelSettings {
  OpenAutofillAndPasswordsSubpage(
      kSettingsTravelInfoCellId,
      grey_allOf(grey_text(l10n_util::GetNSString(
                     IDS_AUTOFILL_TRAVEL_OPT_IN_TOGGLE_LABEL)),
                 grey_kindOfClass([UILabel class]), nil));
}

+ (void)openAutofillSettings {
  OpenAutofillAndPasswordsSubpage(
      kSettingsAutofillSettingsCellId,
      grey_accessibilityID(kEnhancedAutofillSwitchViewId));
}

+ (id<GREYMatcher>)editDoneButton {
  return grey_accessibilityID(kSettingsToolbarEditDoneButtonId);
}

+ (id<GREYMatcher>)textFieldForType:(autofill::AttributeTypeName)type {
  autofill::AttributeType attrType(type);
  return grey_allOf(
      grey_kindOfClass([UITextField class]),
      grey_accessibilityID(base::SysUTF8ToNSString(attrType.name_as_string())),
      grey_ancestor(grey_accessibilityID(kAutofillAIEntityEditTableViewId)),
      nil);
}

+ (void)tapEditDoneButton {
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:[self editDoneButton]];
  [[EarlGrey selectElementWithMatcher:[self editDoneButton]]
      performAction:grey_tap()];
}

+ (void)entityWithLabel:(NSString*)label isVisible:(BOOL)isVisible {
  if (isVisible) {
    [ChromeEarlGrey
        waitForSufficientlyVisibleElementWithMatcher:GetMatcherForLabel(label)];
  } else {
    [ChromeEarlGrey
        waitForUIElementToDisappearWithMatcher:GetMatcherForLabel(label)];
  }
}

+ (void)tapEntityWithLabel:(NSString*)label {
  [[EarlGrey selectElementWithMatcher:GetMatcherForLabel(label)]
      performAction:grey_tap()];
}

+ (void)entityEditViewIsVisible:(BOOL)isVisible {
  id<GREYMatcher> matcher =
      grey_accessibilityID(kAutofillAIEntityEditTableViewId);
  if (isVisible) {
    [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:matcher];
  } else {
    [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:matcher];
  }
}

+ (void)startFieldEditing {
  id<GREYMatcher> editButton = chrome_test_util::SettingsToolbarEditButton();
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:editButton];
  [[EarlGrey selectElementWithMatcher:editButton] performAction:grey_tap()];
}

@end
