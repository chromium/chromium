// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/utils/autofill_and_passwords_item_utils.h"

#import "build/branding_buildflags.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
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
    NSString* accessibility_identifier) {
  TableViewDetailIconItem* detail_item =
      [[TableViewDetailIconItem alloc] initWithType:type];
  detail_item.text = text;
  detail_item.detailText = detail_text;
  detail_item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detail_item.accessibilityTraits |= UIAccessibilityTraitButton;
  detail_item.accessibilityIdentifier = accessibility_identifier;
  detail_item.iconImage = symbol;
  if (IsYourSavedInfoSettingsPageIosEnabled()) {
    detail_item.iconTintColor = [UIColor colorNamed:kTextPrimaryColor];
    detail_item.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;
    detail_item.trailingDetailText = detail_text;
    switch (type) {
      case SettingsItemTypePasswords:
        detail_item.detailText = l10n_util::GetNSString(
            IDS_AUTOFILL_AND_PASSWORDS_PASSWORD_MANAGER_SUMMARY);
        break;
      case SettingsItemTypeAutofillCreditCard:
        detail_item.detailText =
            l10n_util::GetNSString(IDS_AUTOFILL_AND_PASSWORDS_PAYMENTS_SUMMARY);
        break;
      case SettingsItemTypeAutofillProfile:
        detail_item.detailText = l10n_util::GetNSString(
            IDS_AUTOFILL_AND_PASSWORDS_CONTACT_INFO_SUMMARY);
        break;
      case SettingsItemTypeIdentityDocs:
        detail_item.detailText = l10n_util::GetNSString(
            IDS_AUTOFILL_AND_PASSWORDS_IDENTITY_DOCS_SUMMARY);
        break;
      case SettingsItemTypeTravelInfo:
        detail_item.detailText =
            l10n_util::GetNSString(IDS_AUTOFILL_AND_PASSWORDS_TRAVEL_SUMMARY);
        break;
      case SettingsItemTypeShoppingInfo:
        detail_item.detailText =
            l10n_util::GetNSString(IDS_AUTOFILL_AND_PASSWORDS_SHOPPING_SUMMARY);
        break;
      default:
        detail_item.detailText = nil;
        break;
    }
  } else {
    detail_item.iconBackgroundColor = [UIColor colorNamed:kYellow500Color];
    detail_item.iconTintColor = UIColor.whiteColor;
  }
  return detail_item;
}

// Returns the branded version of the Google Services symbol.
UIImage* GetBrandedGoogleServicesSymbol() {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return SettingsRootMulticolorSymbol(SymbolGoogleIcon);
#else
  return SettingsRootSymbol(SymbolGearshape2);
#endif
}

// Creates and returns a configured TableViewDetailIconItem for Enhanced
// Autofill.
TableViewDetailIconItem* EnhancedAutofillDetailItem(NSInteger itemType,
                                                    NSInteger titleId,
                                                    UIImage* iconImage) {
  TableViewDetailIconItem* detailItem =
      [[TableViewDetailIconItem alloc] initWithType:itemType];
  detailItem.text = l10n_util::GetNSString(titleId);
  detailItem.textNumberOfLines = 0;
  detailItem.textFont =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  detailItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
  detailItem.selectionStyle = UITableViewCellSelectionStyleNone;
  detailItem.iconImage = iconImage;
  detailItem.iconTintColor = [UIColor colorNamed:kTextPrimaryColor];
  return detailItem;
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

NSString* IdentityDocsItemDetailText(BOOL enabled) {
  return DetailTextForEnabledState(enabled);
}

NSString* TravelInfoItemDetailText(BOOL enabled) {
  return DetailTextForEnabledState(enabled);
}

NSString* ShoppingInfoItemDetailText(BOOL enabled) {
  return DetailTextForEnabledState(enabled);
}

TableViewDetailIconItem* PasswordsItem(BOOL enabled) {
  NSString* passwordsSectionTitle =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER);

  return DetailItemWithType(SettingsItemTypePasswords, passwordsSectionTitle,
                            PasswordsItemDetailText(enabled),
                            SettingsRootSymbol(SymbolPassword),
                            kSettingsPasswordsCellId);
}

TableViewDetailIconItem* AutofillCreditCardItem(BOOL enabled) {
  NSString* title = l10n_util::GetNSString(
      IsYourSavedInfoSettingsPageIosEnabled() ? IDS_AUTOFILL_PAYMENTS_TITLE
                                              : IDS_AUTOFILL_PAYMENT_METHODS);

  return DetailItemWithType(SettingsItemTypeAutofillCreditCard, title,
                            AutofillCreditCardItemDetailText(enabled),
                            SettingsRootSymbol(SymbolCreditCard),
                            kSettingsPaymentMethodsCellId);
}

TableViewDetailIconItem* AutofillProfileItem(BOOL enabled) {
  NSString* title =
      l10n_util::GetNSString(IsYourSavedInfoSettingsPageIosEnabled()
                                 ? IDS_AUTOFILL_CONTACT_INFO_TITLE
                                 : IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE);

  UIImage* symbol = IsYourSavedInfoSettingsPageIosEnabled()
                        ? SettingsRootSymbol(SymbolEnvelope)
                        : SettingsRootSymbol(SymbolLocation);

  return DetailItemWithType(SettingsItemTypeAutofillProfile, title,
                            AutofillProfileItemDetailText(enabled), symbol,
                            kSettingsAddressesAndMoreCellId);
}

TableViewDetailIconItem* IdentityDocsItem(BOOL enabled) {
  NSString* title = l10n_util::GetNSString(IDS_AUTOFILL_IDENTITY_DOCS_TITLE);
  return DetailItemWithType(SettingsItemTypeIdentityDocs, title,
                            IdentityDocsItemDetailText(enabled),
                            SettingsRootSymbol(SymbolPersonTextRectangle),
                            kSettingsIdentityDocsCellId);
}

TableViewDetailIconItem* TravelInfoItem(BOOL enabled) {
  NSString* title = l10n_util::GetNSString(IDS_AUTOFILL_TRAVEL_TITLE);
  return DetailItemWithType(
      SettingsItemTypeTravelInfo, title, TravelInfoItemDetailText(enabled),
      SettingsRootSymbol(SymbolSuitcase), kSettingsTravelInfoCellId);
}

TableViewDetailIconItem* ShoppingInfoItem(BOOL enabled) {
  NSString* title = l10n_util::GetNSString(IDS_AUTOFILL_SHOPPING_TITLE);
  return DetailItemWithType(
      SettingsItemTypeShoppingInfo, title, ShoppingInfoItemDetailText(enabled),
      SettingsRootSymbol(SymbolCart), kSettingsShoppingInfoCellId);
}

TableViewDetailIconItem* AutofillSettingsItem() {
  NSString* title = l10n_util::GetNSString(IDS_IOS_SETTINGS_AUTOFILL_SETTINGS);
  return DetailItemWithType(SettingsItemTypeAutofillSettings, title, nil,
                            SettingsRootSymbol(SymbolSettings),
                            kSettingsAutofillSettingsCellId);
}

TableViewSwitchItem* EnhancedAutofillSwitchItem(NSInteger itemType,
                                                BOOL enabled,
                                                id target,
                                                SEL action) {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:itemType];
  switchItem.text = l10n_util::GetNSString(
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillAiOnlineModelToggleNewTitle)
          ? IDS_SETTINGS_AUTOFILL_AI_PAGE_TITLE_V2
          : IDS_SETTINGS_AUTOFILL_AI_PAGE_TITLE);
  switchItem.target = target;
  switchItem.selector = action;
  switchItem.on = enabled;
  switchItem.accessibilityIdentifier = kEnhancedAutofillSwitchViewId;
  return switchItem;
}

TableViewHeaderFooterItem* EnhancedAutofillSwitchFooter(NSInteger itemType) {
  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:itemType];
  footer.text = l10n_util::GetNSString(
      base::FeatureList::IsEnabled(autofill::features::kAutofillAiUsePrivateAi)
          ? IDS_SETTINGS_AUTOFILL_AI_TOGGLE_SUB_LABEL_V2
          : IDS_SETTINGS_AUTOFILL_AI_TOGGLE_SUB_LABEL);
  return footer;
}

TableViewHeaderFooterItem* EnhancedAutofillWhenOnSectionHeader(
    NSInteger itemType) {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:itemType];
  header.text = l10n_util::GetNSString(IDS_SETTINGS_AUTOFILL_AI_WHEN_ON);
  return header;
}

TableViewDetailIconItem* EnhancedAutofillCanFillDifficultFieldsItem(
    NSInteger itemType) {
  return EnhancedAutofillDetailItem(
      itemType, IDS_SETTINGS_AUTOFILL_AI_WHEN_ON_CAN_FILL_DIFFICULT_FIELDS,
      SymbolWithPointSize(SymbolTextAnalysis,
                          kSettingsRootSymbolImagePointSize));
}

TableViewHeaderFooterItem* EnhancedAutofillThingsToConsiderSectionHeader(
    NSInteger itemType) {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:itemType];
  header.text =
      l10n_util::GetNSString(IDS_SETTINGS_AUTOFILL_AI_THINGS_TO_CONSIDER);
  return header;
}

TableViewDetailIconItem* EnhancedAutofillDataUsageItem(NSInteger itemType) {
  return EnhancedAutofillDetailItem(
      itemType,
      base::FeatureList::IsEnabled(autofill::features::kAutofillAiUsePrivateAi)
          ? IDS_SETTINGS_AUTOFILL_AI_TO_CONSIDER_DATA_USAGE_V2
          : IDS_SETTINGS_AUTOFILL_AI_TO_CONSIDER_DATA_USAGE,
      MakeSymbolMonochrome(GetBrandedGoogleServicesSymbol()));
}

TableViewDetailIconItem* EnhancedAutofillEnterpriseManagedLoggingDisabledItem(
    NSInteger itemType) {
  return EnhancedAutofillDetailItem(
      itemType, IDS_SETTINGS_AUTOFILL_AI_ENTERPRISE_LOGGING_MANAGED_DISABLED,
      SymbolWithPointSize(SymbolEnterprise, kSettingsRootSymbolImagePointSize));
}

TableViewSwitchItem* AutofillVerificationSwitchItem(NSInteger itemType,
                                                    BOOL enabled,
                                                    BOOL on,
                                                    id target,
                                                    SEL action) {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:itemType];
  switchItem.text =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_VERIFICATION_INFO_LABEL);
  switchItem.target = target;
  switchItem.selector = action;
  switchItem.on = on;
  switchItem.enabled = enabled;
  switchItem.accessibilityIdentifier = kAutofillVerificationSwitchTableViewId;
  return switchItem;
}

TableViewHeaderFooterItem* AutofillVerificationSwitchFooter(
    NSInteger itemType) {
  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:itemType];
  footer.text =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_VERIFICATION_INFO_FOOTER);
  return footer;
}
