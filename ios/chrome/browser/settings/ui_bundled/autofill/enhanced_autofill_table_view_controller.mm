// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/enhanced_autofill_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSwitches = kSectionIdentifierEnumZero,
  SectionIdentifierWhenOn,
  SectionIdentifierThingsToConsider
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeEnhancedAutofillSwitch = kItemTypeEnumZero,
  ItemTypeFooter,
  ItemTypeHeader,
  ItemTypeLabel
};

// Returns the branded version of the Google Services symbol.
UIImage* GetBrandedGoogleServicesSymbol() {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return CustomSettingsRootMulticolorSymbol(kGoogleIconSymbol);
#else
  return DefaultSettingsRootSymbol(kGearshape2Symbol);
#endif
}

}  // namespace

@interface EnhancedAutofillTableViewController () {
  raw_ptr<Browser> _browser;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

@end

@implementation EnhancedAutofillTableViewController

- (instancetype)initWithBrowser:(Browser*)browser {
  CHECK(browser);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _browser = browser;
  }
  return self;
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

  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSwitches];
  [model addItem:[self enhancedAutofillSwitchItem]
      toSectionWithIdentifier:SectionIdentifierSwitches];
  [model setFooter:[self enhancedAutofillSwitchFooter]
      forSectionWithIdentifier:SectionIdentifierSwitches];

  [model addSectionWithIdentifier:SectionIdentifierWhenOn];
  [model setHeader:[self whenOnSectionHeader]
      forSectionWithIdentifier:SectionIdentifierWhenOn];
  [model addItem:[self canFillDifficultFieldsItem]
      toSectionWithIdentifier:SectionIdentifierWhenOn];

  [model addSectionWithIdentifier:SectionIdentifierThingsToConsider];
  [model setHeader:[self thingsToConsiderSectionHeader]
      forSectionWithIdentifier:SectionIdentifierThingsToConsider];
  [model addItem:[self dataUsageItem]
      toSectionWithIdentifier:SectionIdentifierThingsToConsider];
  [model addItem:[self storedOnDeviceItem]
      toSectionWithIdentifier:SectionIdentifierThingsToConsider];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("EnhancedAutofillSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("EnhancedAutofillSettingsBack"));
}

#pragma mark - LoadModel Helpers

- (TableViewItem*)enhancedAutofillSwitchItem {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeEnhancedAutofillSwitch];
  switchItem.text = l10n_util::GetNSString(IDS_SETTINGS_AUTOFILL_AI_PAGE_TITLE);
  switchItem.target = self;
  switchItem.selector = @selector(enhancedAutofillSwitchChanged:);
  switchItem.on = [self isEnhancedAutofillEnabled];
  switchItem.accessibilityIdentifier = kEnhancedAutofillSwitchViewId;
  return switchItem;
}

- (TableViewHeaderFooterItem*)enhancedAutofillSwitchFooter {
  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footer.text =
      l10n_util::GetNSString(IDS_SETTINGS_AUTOFILL_AI_TOGGLE_SUB_LABEL);
  return footer;
}

- (TableViewHeaderFooterItem*)whenOnSectionHeader {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  header.text = l10n_util::GetNSString(IDS_SETTINGS_AUTOFILL_AI_WHEN_ON);
  return header;
}

- (TableViewDetailIconItem*)canFillDifficultFieldsItem {
  return [self detailItemWithTitleId:
                   IDS_SETTINGS_AUTOFILL_AI_WHEN_ON_CAN_FILL_DIFFICULT_FIELDS
                           iconImage:CustomSymbolWithPointSize(
                                         kTextAnalysisSymbol,
                                         kSettingsRootSymbolImagePointSize)];
}

- (TableViewHeaderFooterItem*)thingsToConsiderSectionHeader {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  header.text =
      l10n_util::GetNSString(IDS_SETTINGS_AUTOFILL_AI_THINGS_TO_CONSIDER);
  return header;
}

- (TableViewDetailIconItem*)dataUsageItem {
  return [self
      detailItemWithTitleId:IDS_SETTINGS_AUTOFILL_AI_TO_CONSIDER_DATA_USAGE
                  iconImage:MakeSymbolMonochrome(
                                GetBrandedGoogleServicesSymbol())];
}

- (TableViewDetailIconItem*)storedOnDeviceItem {
  return [self
      detailItemWithTitleId:IDS_IOS_SETTINGS_ENHANCED_AUTOFILL_SAVED_INFORMATION
                  iconImage:CustomSymbolWithPointSize(
                                kRecentTabsSymbol,
                                kSettingsRootSymbolImagePointSize)];
}

- (TableViewDetailIconItem*)detailItemWithTitleId:(NSInteger)titleId
                                        iconImage:(UIImage*)iconImage {
  TableViewDetailIconItem* detailItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeLabel];
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

#pragma mark - Getters and Setter

- (BOOL)isEnhancedAutofillEnabled {
  return autofill::IsEnhancedAutofillEnabled(_browser->GetProfile());
}

- (void)setEnhancedAutofillEnabled:(BOOL)isEnabled {
  autofill::SetEnhancedAutofillEnabled(_browser->GetProfile(), isEnabled);
}

#pragma mark - Switch Callbacks

- (void)enhancedAutofillSwitchChanged:(UISwitch*)switchView {
  BOOL enabled = switchView.on;
  [self setSwitchItemOn:enabled itemType:ItemTypeEnhancedAutofillSwitch];
  [self setEnhancedAutofillEnabled:enabled];
}

#pragma mark - Switch Helpers

// Sets switchItem's state to `on`. It is important that there is only one item
// of `switchItemType` in SectionIdentifierSwitches.
- (void)setSwitchItemOn:(BOOL)on itemType:(ItemType)switchItemType {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:switchItemType
                              sectionIdentifier:SectionIdentifierSwitches];
  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  switchItem.on = on;
}

// Sets switchItem's enabled status to `enabled` and reconfigures the
// corresponding cell. It is important that there is no more than one item of
// `switchItemType` in SectionIdentifierSwitches.
- (void)setSwitchItemEnabled:(BOOL)enabled itemType:(ItemType)switchItemType {
  TableViewModel* model = self.tableViewModel;

  if (![model hasItemForItemType:switchItemType
               sectionIdentifier:SectionIdentifierSwitches]) {
    return;
  }
  NSIndexPath* switchPath =
      [model indexPathForItemType:switchItemType
                sectionIdentifier:SectionIdentifierSwitches];
  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [model itemAtIndexPath:switchPath]);
  [switchItem setEnabled:enabled];
  [self reconfigureCellsForItems:@[ switchItem ]];
}

#pragma mark - SettingsControllerProtocol

- (void)settingsWillBeDismissed {
  _settingsAreDismissed = YES;
}

@end
