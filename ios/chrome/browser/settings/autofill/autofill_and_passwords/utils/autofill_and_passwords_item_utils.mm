// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/utils/autofill_and_passwords_item_utils.h"

#import "base/feature_list.h"
#import "build/branding_buildflags.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Creates and returns a configured TableViewDetailIconItem.
TableViewDetailIconItem* DetailItemWithType(NSInteger type,
                                            NSString* text,
                                            NSString* detailText,
                                            NSString* trailingDetailText,
                                            UIImage* symbol,
                                            NSString* accessibilityIdentifier) {
  TableViewDetailIconItem* detailItem =
      [[TableViewDetailIconItem alloc] initWithType:type];
  detailItem.text = text;
  detailItem.detailText = detailText;
  detailItem.trailingDetailText = trailingDetailText;
  detailItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  detailItem.accessibilityTraits |= UIAccessibilityTraitButton;
  detailItem.accessibilityIdentifier = accessibilityIdentifier;
  detailItem.iconImage = symbol;
  detailItem.iconTintColor = [UIColor colorNamed:kTextPrimaryColor];
  detailItem.textLayoutConstraintAxis = UILayoutConstraintAxisVertical;
  return detailItem;
}

// Returns the branded version of the Google Services symbol.
UIImage* GetBrandedGoogleServicesSymbol() {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return SettingsRootSymbol(SymbolGoogleIconMonochrome);
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

NSString* DetailTextForEnabledState(BOOL enabled) {
  return enabled ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                 : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
}

TableViewDetailIconItem* PasswordsItem(BOOL enabled) {
  NSString* title = l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER);
  NSString* summary = l10n_util::GetNSString(
      IDS_AUTOFILL_AND_PASSWORDS_PASSWORD_MANAGER_SUMMARY);
  return DetailItemWithType(SettingsItemTypePasswords, title, summary,
                            DetailTextForEnabledState(enabled),
                            SettingsRootSymbol(SymbolPassword),
                            kSettingsPasswordsCellId);
}

TableViewDetailIconItem* AutofillCreditCardItem(BOOL enabled) {
  NSString* title = l10n_util::GetNSString(IDS_AUTOFILL_PAYMENTS_TITLE);
  NSString* summary =
      l10n_util::GetNSString(IDS_AUTOFILL_AND_PASSWORDS_PAYMENTS_SUMMARY);
  return DetailItemWithType(SettingsItemTypeAutofillCreditCard, title, summary,
                            DetailTextForEnabledState(enabled),
                            SettingsRootSymbol(SymbolCreditCard),
                            kSettingsPaymentMethodsCellId);
}

TableViewDetailIconItem* AutofillProfileItem(BOOL enabled) {
  NSString* title = l10n_util::GetNSString(IDS_AUTOFILL_CONTACT_INFO_TITLE);
  NSString* summary =
      l10n_util::GetNSString(IDS_AUTOFILL_AND_PASSWORDS_CONTACT_INFO_SUMMARY);
  return DetailItemWithType(SettingsItemTypeAutofillProfile, title, summary,
                            DetailTextForEnabledState(enabled),
                            SettingsRootSymbol(SymbolEnvelope),
                            kSettingsAddressesAndMoreCellId);
}

TableViewDetailIconItem* IdentityDocsItem(BOOL enabled) {
  NSString* title = l10n_util::GetNSString(IDS_AUTOFILL_IDENTITY_DOCS_TITLE);
  NSString* summary =
      l10n_util::GetNSString(IDS_AUTOFILL_AND_PASSWORDS_IDENTITY_DOCS_SUMMARY);
  return DetailItemWithType(SettingsItemTypeIdentityDocs, title, summary,
                            DetailTextForEnabledState(enabled),
                            SettingsRootSymbol(SymbolPersonTextRectangle),
                            kSettingsIdentityDocsCellId);
}

TableViewDetailIconItem* TravelInfoItem(BOOL enabled) {
  NSString* title = l10n_util::GetNSString(IDS_AUTOFILL_TRAVEL_TITLE);
  NSString* summary =
      l10n_util::GetNSString(IDS_AUTOFILL_AND_PASSWORDS_TRAVEL_SUMMARY);
  return DetailItemWithType(SettingsItemTypeTravelInfo, title, summary,
                            DetailTextForEnabledState(enabled),
                            SettingsRootSymbol(SymbolSuitcase),
                            kSettingsTravelInfoCellId);
}

TableViewDetailIconItem* ShoppingInfoItem(BOOL enabled) {
  NSString* title = l10n_util::GetNSString(IDS_AUTOFILL_SHOPPING_TITLE);
  NSString* summary =
      l10n_util::GetNSString(IDS_AUTOFILL_AND_PASSWORDS_SHOPPING_SUMMARY);
  return DetailItemWithType(SettingsItemTypeShoppingInfo, title, summary,
                            DetailTextForEnabledState(enabled),
                            SettingsRootSymbol(SymbolCart),
                            kSettingsShoppingInfoCellId);
}

TableViewDetailIconItem* SuggestionsFromGeminiItem(BOOL enabled) {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_TITLE);
  NSString* summary = l10n_util::GetNSString(
      IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_SUBPAGE_SUMMARY);
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  UIImage* image = SettingsRootSymbol(SymbolGeminiBrandedLogo);
#else
  UIImage* image = SettingsRootSymbol(SymbolGeminiNonBrandedLogo);
#endif
  return DetailItemWithType(SettingsItemTypeSuggestionsFromGemini, title,
                            summary, DetailTextForEnabledState(enabled), image,
                            kSettingsSuggestionsFromGeminiCellId);
}

TableViewDetailIconItem* AutofillSettingsItem() {
  NSString* title = l10n_util::GetNSString(IDS_IOS_SETTINGS_AUTOFILL_SETTINGS);
  return DetailItemWithType(SettingsItemTypeAutofillSettings, title, nil, nil,
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
      GetBrandedGoogleServicesSymbol());
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
