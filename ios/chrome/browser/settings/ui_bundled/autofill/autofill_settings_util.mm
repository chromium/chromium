// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_util.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation AutofillSettingsUtil

+ (void)updateAccessibilityLabelForItem:(TableViewTextEditItem*)item
                           isInputValid:(BOOL)isValid
                           errorMessage:(NSString*)errorMessage {
  NSMutableArray* labelParts = [NSMutableArray array];

  if (item.fieldNameLabelText.length > 0) {
    [labelParts addObject:item.fieldNameLabelText];
  }

  // Essential Placeholder. By default, iOS stops announcing the placeholder
  // once a text field has a value. However, for these specific fields, the
  // placeholder conveys critical constraints that shouldn't disappear. This
  // block checks if the field has text (meaning the default announcement is
  // gone) and manually appends the placeholder back to the accessibility label
  // if it matches one of the essential types.
  if (item.textFieldValue.length > 0) {
    NSString* optionalPlaceholder = l10n_util::GetNSString(
        IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_CVC_OPTIONAL);
    NSString* monthPlaceholder = l10n_util::GetNSString(
        IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_MONTH);
    NSString* yearPlaceholder = l10n_util::GetNSString(
        IDS_IOS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRATION_YEAR);

    BOOL isEssential =
        [item.textFieldPlaceholder isEqualToString:optionalPlaceholder] ||
        [item.textFieldPlaceholder isEqualToString:monthPlaceholder] ||
        [item.textFieldPlaceholder isEqualToString:yearPlaceholder];

    if (isEssential) {
      [labelParts addObject:item.textFieldPlaceholder];
    }
  }

  if (!isValid && errorMessage.length > 0) {
    [labelParts addObject:errorMessage];
  }

  item.cellAccessibilityLabel = [labelParts componentsJoinedByString:@", "];
}

+ (NSString*)errorMessageForUIType:(AutofillCreditCardUIType)type {
  switch (type) {
    case AutofillCreditCardUIType::kNumber:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_INVALID_CARD_NUMBER_ACCESSIBILITY_ANNOUNCEMENT);
    case AutofillCreditCardUIType::kExpMonth:
    case AutofillCreditCardUIType::kExpYear:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_INVALID_EXPIRATION_DATE_ACCESSIBILITY_ANNOUNCEMENT);
    case AutofillCreditCardUIType::kSecurityCode:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_INVALID_CVC_ACCESSIBILITY_ANNOUNCEMENT);
    case AutofillCreditCardUIType::kFullName:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_INVALID_CARDHOLDER_NAME_ACCESSIBILITY_ANNOUNCEMENT);
    case AutofillCreditCardUIType::kNickname:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_INVALID_NICKNAME_ACCESSIBILITY_ANNOUNCEMENT);
    default:
      return nil;
  }
}
@end
