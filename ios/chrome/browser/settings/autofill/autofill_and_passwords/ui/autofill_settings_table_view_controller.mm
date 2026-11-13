// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_settings_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/feature_list.h"
#import "base/metrics/user_metrics.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

enum class SectionIdentifier {
  kSwitchesSection = kSectionIdentifierEnumZero,
  kWhenOnSection,
  kThingsToConsiderSection,
  kVerificationSwitchSection,
  kWalletPromoSection,
};

enum class ItemType {
  kEnhancedAutofillSwitchItem = kItemTypeEnumZero,
  kVerificationSwitchItem,
  kVerificationFooterItem,
  kFooterItem,
  kHeaderItem,
  kLabelItem,
  kWalletPromoInfoItem,
  kWalletPromoButtonItem,
};

// Returns the branded version of the Google Services symbol.
UIImage* GetBrandedGoogleServicesSymbol() {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return SettingsRootSymbol(SymbolGoogleIconMonochrome);
#else
  return SettingsRootSymbol(SymbolGearshape2);
#endif
}

// Returns a configured `TableViewDetailIconItem` for Enhanced Autofill.
TableViewDetailIconItem* EnhancedAutofillDetailItem(int title_id,
                                                    UIImage* icon_image) {
  TableViewDetailIconItem* detail_item = [[TableViewDetailIconItem alloc]
      initWithType:static_cast<NSInteger>(ItemType::kLabelItem)];
  detail_item.text = l10n_util::GetNSString(title_id);
  detail_item.textNumberOfLines = 0;
  detail_item.textFont =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  detail_item.textColor = [UIColor colorNamed:kTextSecondaryColor];
  detail_item.selectionStyle = UITableViewCellSelectionStyleNone;
  detail_item.iconImage = icon_image;
  detail_item.iconTintColor = [UIColor colorNamed:kTextPrimaryColor];
  return detail_item;
}

}  // namespace

@implementation AutofillSettingsTableViewController {
  BOOL _settingsAreDismissed;
  BOOL _enhancedAutofillEnabled;
  BOOL _autofillAIAllowedByPolicy;
  BOOL _userVerificationEnabled;
  BOOL _userVerificationSwitchEnabled;
  BOOL _userVerificationSettingVisible;
  BOOL _shouldShowWalletPromo;
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_IOS_SETTINGS_AUTOFILL_SETTINGS);
  }
  return self;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate autofillSettingsTableViewControllerDidRemove:self];
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  if (_settingsAreDismissed) {
    return;
  }

  TableViewModel<TableViewItem*>* model = self.tableViewModel;

  [model addSectionWithIdentifier:static_cast<NSInteger>(
                                      SectionIdentifier::kSwitchesSection)];
  [model addItem:[self enhancedAutofillSwitchItem]
      toSectionWithIdentifier:static_cast<NSInteger>(
                                  SectionIdentifier::kSwitchesSection)];
  [model setFooter:[self enhancedAutofillSwitchFooter]
      forSectionWithIdentifier:static_cast<NSInteger>(
                                   SectionIdentifier::kSwitchesSection)];

  [model addSectionWithIdentifier:static_cast<NSInteger>(
                                      SectionIdentifier::kWhenOnSection)];
  [model setHeader:[self whenOnSectionHeader]
      forSectionWithIdentifier:static_cast<NSInteger>(
                                   SectionIdentifier::kWhenOnSection)];
  [model addItem:[self canFillDifficultFieldsItem]
      toSectionWithIdentifier:static_cast<NSInteger>(
                                  SectionIdentifier::kWhenOnSection)];

  [model
      addSectionWithIdentifier:
          static_cast<NSInteger>(SectionIdentifier::kThingsToConsiderSection)];
  [model setHeader:[self thingsToConsiderSectionHeader]
      forSectionWithIdentifier:
          static_cast<NSInteger>(SectionIdentifier::kThingsToConsiderSection)];
  [model addItem:[self dataUsageItem]
      toSectionWithIdentifier:static_cast<NSInteger>(
                                  SectionIdentifier::kThingsToConsiderSection)];

  if (!_autofillAIAllowedByPolicy) {
    [model addItem:[self enterpriseManagedLoggingDisabledItem]
        toSectionWithIdentifier:
            static_cast<NSInteger>(
                SectionIdentifier::kThingsToConsiderSection)];
  }

  if (_userVerificationSettingVisible) {
    [model addSectionWithIdentifier:
               static_cast<NSInteger>(
                   SectionIdentifier::kVerificationSwitchSection)];
    [model addItem:[self verificationSwitchItem]
        toSectionWithIdentifier:
            static_cast<NSInteger>(
                SectionIdentifier::kVerificationSwitchSection)];
    [model setFooter:[self verificationSwitchFooter]
        forSectionWithIdentifier:
            static_cast<NSInteger>(
                SectionIdentifier::kVerificationSwitchSection)];
  }

  if (_shouldShowWalletPromo) {
    [model
        addSectionWithIdentifier:static_cast<NSInteger>(
                                     SectionIdentifier::kWalletPromoSection)];
    [model addItem:[self walletPromoInfoItem]
        toSectionWithIdentifier:static_cast<NSInteger>(
                                    SectionIdentifier::kWalletPromoSection)];
    [model addItem:[self walletPromoButtonItem]
        toSectionWithIdentifier:static_cast<NSInteger>(
                                    SectionIdentifier::kWalletPromoSection)];
  }
}

#pragma mark - AutofillSettingsConsumer

- (void)setEnhancedAutofillEnabled:(BOOL)enabled {
  if (_enhancedAutofillEnabled == enabled) {
    return;
  }
  _enhancedAutofillEnabled = enabled;
  if (self.isViewLoaded) {
    [self setSwitchItemOn:enabled
                 itemType:ItemType::kEnhancedAutofillSwitchItem
        sectionIdentifier:SectionIdentifier::kSwitchesSection];
  }
}

- (void)setAutofillAIAllowedByPolicy:(BOOL)allowed {
  if (_autofillAIAllowedByPolicy == allowed) {
    return;
  }
  _autofillAIAllowedByPolicy = allowed;
  if (self.isViewLoaded) {
    [self reloadData];
  }
}

- (void)setUserVerificationEnabled:(BOOL)enabled {
  _userVerificationEnabled = enabled;
  if (self.isViewLoaded) {
    [self setSwitchItemOn:enabled
                 itemType:ItemType::kVerificationSwitchItem
        sectionIdentifier:SectionIdentifier::kVerificationSwitchSection];
  }
}

- (void)setUserVerificationSwitchEnabled:(BOOL)enabled {
  if (_userVerificationSwitchEnabled == enabled) {
    return;
  }
  _userVerificationSwitchEnabled = enabled;
  if (self.isViewLoaded) {
    [self updateVerificationSwitchEnabledState];
  }
}

- (void)setUserVerificationSettingVisible:(BOOL)visible {
  if (_userVerificationSettingVisible == visible) {
    return;
  }
  _userVerificationSettingVisible = visible;
  if (self.isViewLoaded) {
    [self reloadData];
  }
}

- (void)setShouldShowWalletPromo:(BOOL)shouldShowWalletPromo {
  if (_shouldShowWalletPromo == shouldShowWalletPromo) {
    return;
  }
  _shouldShowWalletPromo = shouldShowWalletPromo;
  if (self.isViewLoaded) {
    [self reloadData];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  if (_settingsAreDismissed) {
    return;
  }

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  if (itemType == static_cast<NSInteger>(ItemType::kWalletPromoButtonItem)) {
    [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
    [self.delegate
        autofillSettingsTableViewControllerDidTapWalletPromoCard:self];
  }
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAutofillSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAutofillSettingsBack"));
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  _settingsAreDismissed = YES;
}

#pragma mark - Private

// Returns the switch item for Enhanced Autofill.
- (TableViewSwitchItem*)enhancedAutofillSwitchItem {
  TableViewSwitchItem* switchItem = [[TableViewSwitchItem alloc]
      initWithType:static_cast<NSInteger>(
                       ItemType::kEnhancedAutofillSwitchItem)];
  switchItem.text = l10n_util::GetNSString(
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillAiOnlineModelToggleNewTitle)
          ? IDS_SETTINGS_AUTOFILL_AI_PAGE_TITLE_V2
          : IDS_SETTINGS_AUTOFILL_AI_PAGE_TITLE);
  switchItem.target = self;
  switchItem.selector = @selector(enhancedAutofillSwitchChanged:);
  switchItem.on = _enhancedAutofillEnabled;
  switchItem.accessibilityIdentifier = kEnhancedAutofillSwitchViewId;
  return switchItem;
}

// Returns the footer item for the Enhanced Autofill switch section.
- (TableViewLinkHeaderFooterItem*)enhancedAutofillSwitchFooter {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:static_cast<NSInteger>(ItemType::kFooterItem)];
  footer.text = l10n_util::GetNSString(
      base::FeatureList::IsEnabled(autofill::features::kAutofillAiUsePrivateAi)
          ? IDS_SETTINGS_AUTOFILL_AI_TOGGLE_SUB_LABEL_V2
          : IDS_SETTINGS_AUTOFILL_AI_TOGGLE_SUB_LABEL);
  return footer;
}

// Returns the header item for the "When on" section.
- (TableViewTextHeaderFooterItem*)whenOnSectionHeader {
  TableViewTextHeaderFooterItem* header = [[TableViewTextHeaderFooterItem alloc]
      initWithType:static_cast<NSInteger>(ItemType::kHeaderItem)];
  header.text = l10n_util::GetNSString(IDS_SETTINGS_AUTOFILL_AI_WHEN_ON);
  return header;
}

// Returns the detail icon item for "Can fill difficult fields".
- (TableViewDetailIconItem*)canFillDifficultFieldsItem {
  return EnhancedAutofillDetailItem(
      IDS_SETTINGS_AUTOFILL_AI_WHEN_ON_CAN_FILL_DIFFICULT_FIELDS,
      SettingsRootSymbol(SymbolTextAnalysis));
}

// Returns the header item for the "Things to consider" section.
- (TableViewTextHeaderFooterItem*)thingsToConsiderSectionHeader {
  TableViewTextHeaderFooterItem* header = [[TableViewTextHeaderFooterItem alloc]
      initWithType:static_cast<NSInteger>(ItemType::kHeaderItem)];
  header.text =
      l10n_util::GetNSString(IDS_SETTINGS_AUTOFILL_AI_THINGS_TO_CONSIDER);
  return header;
}

// Returns the detail icon item for "Data usage".
- (TableViewDetailIconItem*)dataUsageItem {
  return EnhancedAutofillDetailItem(
      base::FeatureList::IsEnabled(autofill::features::kAutofillAiUsePrivateAi)
          ? IDS_SETTINGS_AUTOFILL_AI_TO_CONSIDER_DATA_USAGE_V2
          : IDS_SETTINGS_AUTOFILL_AI_TO_CONSIDER_DATA_USAGE,
      GetBrandedGoogleServicesSymbol());
}

// Returns the detail icon item for "Enterprise managed logging disabled".
- (TableViewDetailIconItem*)enterpriseManagedLoggingDisabledItem {
  return EnhancedAutofillDetailItem(
      IDS_SETTINGS_AUTOFILL_AI_ENTERPRISE_LOGGING_MANAGED_DISABLED,
      SettingsRootSymbol(SymbolEnterprise));
}

// Returns the switch item for Autofill AI user verification.
- (TableViewSwitchItem*)verificationSwitchItem {
  TableViewSwitchItem* switchItem = [[TableViewSwitchItem alloc]
      initWithType:static_cast<NSInteger>(ItemType::kVerificationSwitchItem)];
  switchItem.text =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_VERIFICATION_INFO_LABEL);
  switchItem.target = self;
  switchItem.selector = @selector(verificationSwitchChanged:);
  switchItem.on = _userVerificationEnabled;
  switchItem.enabled = _userVerificationSwitchEnabled;
  switchItem.accessibilityIdentifier = kAutofillVerificationSwitchTableViewId;
  return switchItem;
}

// Returns the footer item for the Autofill AI user verification switch section.
- (TableViewLinkHeaderFooterItem*)verificationSwitchFooter {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:static_cast<NSInteger>(ItemType::kVerificationFooterItem)];
  footer.text =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_VERIFICATION_INFO_FOOTER);
  return footer;
}

// Returns the Google Wallet promo info item.
- (TableViewItem*)walletPromoInfoItem {
  TableViewDetailTextItem* item = [[TableViewDetailTextItem alloc]
      initWithType:static_cast<NSInteger>(ItemType::kWalletPromoInfoItem)];
  item.text = l10n_util::GetNSString(IDS_IOS_AUTOFILL_WALLET_PROMO_TITLE);
  item.detailText =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_WALLET_PROMO_DETAIL_TEXT);
  item.allowMultilineDetailText = YES;
  return item;
}

// Returns the Google Wallet promo button item.
- (TableViewItem*)walletPromoButtonItem {
  TableViewTextItem* item = [[TableViewTextItem alloc]
      initWithType:static_cast<NSInteger>(ItemType::kWalletPromoButtonItem)];
  item.text = l10n_util::GetNSString(IDS_IOS_AUTOFILL_WALLET_PROMO_LINK_TEXT);
  item.textColor = [UIColor colorNamed:kBlueColor];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  item.titleNumberOfLines = 0;
  return item;
}

- (void)enhancedAutofillSwitchChanged:(UISwitch*)switchView {
  [self.mutator setEnhancedAutofillEnabled:switchView.on];
}

- (void)verificationSwitchChanged:(UISwitch*)switchView {
  [self.mutator setUserVerificationEnabled:switchView.on];
}

- (void)setSwitchItemOn:(BOOL)on
               itemType:(ItemType)switchItemType
      sectionIdentifier:(SectionIdentifier)sectionIdentifier {
  if (![self.tableViewModel
          hasItemForItemType:static_cast<NSInteger>(switchItemType)
           sectionIdentifier:static_cast<NSInteger>(sectionIdentifier)]) {
    return;
  }
  NSIndexPath* switchPath = [self.tableViewModel
      indexPathForItemType:static_cast<NSInteger>(switchItemType)
         sectionIdentifier:static_cast<NSInteger>(sectionIdentifier)];
  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  switchItem.on = on;
  [self reconfigureCellsForItems:@[ switchItem ]];
}

- (void)updateVerificationSwitchEnabledState {
  if (![self.tableViewModel
          hasItemForItemType:static_cast<NSInteger>(
                                 ItemType::kVerificationSwitchItem)
           sectionIdentifier:
               static_cast<NSInteger>(
                   SectionIdentifier::kVerificationSwitchSection)]) {
    return;
  }
  NSIndexPath* switchPath = [self.tableViewModel
      indexPathForItemType:static_cast<NSInteger>(
                               ItemType::kVerificationSwitchItem)
         sectionIdentifier:static_cast<NSInteger>(
                               SectionIdentifier::kVerificationSwitchSection)];
  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  switchItem.enabled = _userVerificationSwitchEnabled;
  [self reconfigureCellsForItems:@[ switchItem ]];
}

@end
