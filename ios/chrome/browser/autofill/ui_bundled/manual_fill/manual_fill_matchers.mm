// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_matchers.h"

#import "ios/chrome/browser/autofill/model/form_suggestion_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace manual_fill {

id<GREYMatcher> FormSuggestionViewMatcher() {
  return grey_accessibilityID(kFormSuggestionsViewAccessibilityIdentifier);
}

id<GREYMatcher> PasswordIconMatcher() {
  return grey_accessibilityID(kAccessoryPasswordAccessibilityIdentifier);
}

id<GREYMatcher> KeyboardIconMatcher() {
  return grey_accessibilityID(kAccessoryKeyboardAccessibilityIdentifier);
}

id<GREYMatcher> PasswordTableViewMatcher() {
  return grey_accessibilityID(kPasswordTableViewAccessibilityIdentifier);
}

id<GREYMatcher> PasswordSearchBarMatcher() {
  return grey_accessibilityID(kPasswordSearchBarAccessibilityIdentifier);
}

id<GREYMatcher> ManagePasswordsMatcher() {
  return grey_accessibilityID(kManagePasswordsAccessibilityIdentifier);
}

id<GREYMatcher> ManageSettingsMatcher() {
  return grey_accessibilityID(kManageSettingsAccessibilityIdentifier);
}

id<GREYMatcher> OtherPasswordsMatcher() {
  return grey_accessibilityID(kOtherPasswordsAccessibilityIdentifier);
}

id<GREYMatcher> OtherPasswordsDismissMatcher() {
  return grey_accessibilityID(kPasswordDoneButtonAccessibilityIdentifier);
}

id<GREYMatcher> PasswordButtonMatcher() {
  return grey_buttonTitle(kMaskedPasswordButtonText);
}

id<GREYMatcher> ProfilesIconMatcher() {
  return grey_accessibilityID(kAccessoryAddressAccessibilityIdentifier);
}

id<GREYMatcher> ProfilesTableViewMatcher() {
  return grey_accessibilityID(kAddressTableViewAccessibilityIdentifier);
}

id<GREYMatcher> ManageProfilesMatcher() {
  return grey_accessibilityID(kManageAddressAccessibilityIdentifier);
}

id<GREYMatcher> ProfileTableViewWindowMatcher() {
  id<GREYMatcher> classMatcher = grey_kindOfClass([UIWindow class]);
  id<GREYMatcher> parentMatcher = grey_descendant(ProfilesTableViewMatcher());
  return grey_allOf(classMatcher, parentMatcher, nil);
}

id<GREYMatcher> CreditCardIconMatcher() {
  return grey_accessibilityID(kAccessoryCreditCardAccessibilityIdentifier);
}

id<GREYMatcher> CreditCardTableViewMatcher() {
  return grey_accessibilityID(kCardTableViewAccessibilityIdentifier);
}

id<GREYMatcher> ManagePaymentMethodsMatcher() {
  return grey_accessibilityID(kManagePaymentMethodsAccessibilityIdentifier);
}

id<GREYMatcher> AddPaymentMethodMatcher() {
  return grey_accessibilityID(kAddPaymentMethodAccessibilityIdentifier);
}

id<GREYMatcher> CreditCardTableViewWindowMatcher() {
  id<GREYMatcher> classMatcher = grey_kindOfClass([UIWindow class]);
  id<GREYMatcher> parentMatcher = grey_descendant(CreditCardTableViewMatcher());
  return grey_allOf(classMatcher, parentMatcher, nil);
}

id<GREYMatcher> SuggestPasswordMatcher() {
  return grey_accessibilityID(kSuggestPasswordAccessibilityIdentifier);
}

id<GREYMatcher> ExpandedManualFillView() {
  return grey_accessibilityID(manual_fill::kExpandedManualFillViewID);
}

id<GREYMatcher> KeyboardAccessoryManualFillButton() {
  return grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_AUTOFILL_DATA));
}

id<GREYMatcher> ExpandedManualFillHeaderView() {
  return grey_accessibilityID(manual_fill::kExpandedManualFillHeaderViewID);
}

id<GREYMatcher> SegmentedControlAddressTab() {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_EXPANDED_MANUAL_FILL_ADDRESS_TAB_ACCESSIBILITY_LABEL)),
      grey_ancestor(ExpandedManualFillHeaderView()), nil);
}

id<GREYMatcher> SegmentedControlPasswordTab() {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_EXPANDED_MANUAL_FILL_PASSWORD_TAB_ACCESSIBILITY_LABEL)),
      grey_ancestor(ExpandedManualFillHeaderView()), nil);
}

id<GREYMatcher> ChipButton(std::u16string title) {
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabel(l10n_util::GetNSStringF(
          IDS_IOS_MANUAL_FALLBACK_CHIP_ACCESSIBILITY_LABEL, title)),
      grey_interactable(), nullptr);
}

id<GREYMatcher> PasswordManualFillViewButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_AUTOFILL_PASSWORD_AUTOFILL_DATA)),
                    grey_ancestor(grey_accessibilityID(
                        kFormInputAccessoryViewAccessibilityID)),
                    nil);
}

}  // namespace manual_fill
