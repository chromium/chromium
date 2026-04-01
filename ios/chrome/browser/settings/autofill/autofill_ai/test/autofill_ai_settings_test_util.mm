// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/test/autofill_ai_settings_test_util.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/public/autofill_ai_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

// Label within a table view cell in "Addresses and more" settings.
id<GREYMatcher> GetMatcherForLabel(NSString* label) {
  return grey_allOf(
      grey_accessibilityLabel(label),
      grey_ancestor(grey_accessibilityID(kAutofillProfileTableViewID)),
      grey_interactable(), nil);
}

}  // namespace

@implementation AutofillAISettingsTestUtil

+ (void)openPreHoTLocation {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::AddressesAndMoreButton()];
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
  [[EarlGrey selectElementWithMatcher:[self editDoneButton]]
      performAction:grey_tap()];
}

+ (void)entityWithLabel:(NSString*)label isVisible:(BOOL)isVisible {
  [[EarlGrey selectElementWithMatcher:GetMatcherForLabel(label)]
      assertWithMatcher:isVisible ? grey_sufficientlyVisible()
                                  : grey_notVisible()];
}

+ (void)tapEntityWithLabel:(NSString*)label {
  [[EarlGrey selectElementWithMatcher:GetMatcherForLabel(label)]
      performAction:grey_tap()];
}

+ (void)entityEditViewIsVisible:(BOOL)isVisible {
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAutofillAIEntityEditTableViewId)]
      assertWithMatcher:isVisible ? grey_sufficientlyVisible()
                                  : grey_notVisible()];
}

+ (void)startFieldEditing {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsToolbarEditButton()]
      performAction:grey_tap()];
}

@end
