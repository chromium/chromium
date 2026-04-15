// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/utils/autofill_and_passwords_item_utils.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Returns the localized "On" or "Off" text based on the `enabled` state.
NSString* DetailTextForEnabledState(BOOL enabled) {
  return enabled ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                 : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
}

// Creates and returns a configured TableViewDetailIconItem.
TableViewDetailIconItem* DetailItemWithType(
    NSInteger type,
    NSString* text,
    NSString* detail_text,
    UIImage* symbol,
    UIColor* background_color,
    NSString* accessibility_identifier) {
  TableViewDetailIconItem* detail_item =
      [[TableViewDetailIconItem alloc] initWithType:type];
  detail_item.text = text;
  detail_item.detailText = detail_text;
  detail_item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detail_item.accessibilityTraits |= UIAccessibilityTraitButton;
  detail_item.accessibilityIdentifier = accessibility_identifier;
  detail_item.iconImage = symbol;
  if (background_color) {
    detail_item.iconBackgroundColor = background_color;
    detail_item.iconTintColor = UIColor.whiteColor;
  }
  return detail_item;
}

}  // namespace

NSString* PasswordsItemDetailText(BOOL enabled) {
  return DetailTextForEnabledState(enabled);
}

NSString* AutofillCreditCardItemDetailText(BOOL enabled) {
  return DetailTextForEnabledState(enabled);
}

NSString* AutofillProfileItemDetailText(BOOL enabled) {
  return DetailTextForEnabledState(enabled);
}

TableViewDetailIconItem* PasswordsItem(BOOL enabled) {
  NSString* passwordsSectionTitle =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER);

  return DetailItemWithType(SettingsItemTypePasswords, passwordsSectionTitle,
                            PasswordsItemDetailText(enabled),
                            CustomSettingsRootSymbol(kPasswordSymbol),
                            [UIColor colorNamed:kYellow500Color],
                            kSettingsPasswordsCellId);
}

TableViewDetailIconItem* AutofillCreditCardItem(BOOL enabled) {
  NSString* title = l10n_util::GetNSString(
      IsYourSavedInfoSettingsPageIosEnabled() ? IDS_AUTOFILL_PAYMENTS_TITLE
                                              : IDS_AUTOFILL_PAYMENT_METHODS);

  return DetailItemWithType(SettingsItemTypeAutofillCreditCard, title,
                            AutofillCreditCardItemDetailText(enabled),
                            DefaultSettingsRootSymbol(kCreditCardSymbol),
                            [UIColor colorNamed:kYellow500Color],
                            kSettingsPaymentMethodsCellId);
}

TableViewDetailIconItem* AutofillProfileItem(BOOL enabled) {
  NSString* title = l10n_util::GetNSString(
      IsYourSavedInfoSettingsPageIosEnabled()
          ? IDS_AUTOFILL_CONTACT_INFO_TITLE
          : IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE);

  return DetailItemWithType(
      SettingsItemTypeAutofillProfile, title,
      AutofillProfileItemDetailText(enabled),
      CustomSettingsRootSymbol(kLocationSymbol),
      [UIColor colorNamed:kYellow500Color], kSettingsAddressesAndMoreCellId);
}
