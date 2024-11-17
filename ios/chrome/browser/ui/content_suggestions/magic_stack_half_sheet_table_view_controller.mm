// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/parcel_tracking/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_model_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The point size of the icons.
const CGFloat kIconPointSize = 18;

enum SectionIdentifier : NSInteger {
  SectionIdentifierOptions = kSectionIdentifierEnumZero,
};

enum ItemType : NSInteger {
  ItemTypeToggleSetUpList = kItemTypeEnumZero,
  ItemTypeToggleSafetyCheck,
  ItemTypeToggleTabResumption,
  ItemTypeToggleParcelTracking,
};

}  // namespace

@interface MagicStackHalfSheetTableViewController ()
@end

@implementation MagicStackHalfSheetTableViewController {
  BOOL _showSetUpList;
  BOOL _setUpListDisabled;
  BOOL _safetyCheckDisabled;
  BOOL _tabResumptionDisabled;
  BOOL _parcelTrackingDisabled;

  TableViewSwitchItem* _setUpListToggle;
  TableViewSwitchItem* _safetyCheckToggle;
  TableViewSwitchItem* _tabResumptionToggle;
  TableViewSwitchItem* _parcelTrackingToggle;
}

- (instancetype)init {
  UITableViewStyle style = ChromeTableViewStyle();
  return [super initWithStyle:style];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_EDIT_MODAL_TITLE);
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self.delegate
                           action:@selector(dismissMagicStackHalfSheet)];
  dismissButton.accessibilityIdentifier =
      kMagicStackEditHalfSheetDoneButtonAccessibilityIdentifier;
  self.navigationItem.rightBarButtonItem = dismissButton;

  [self loadModel];
}

#pragma mark - MagicStackHalfSheetConsumer

- (void)showSetUpList:(BOOL)showSetUpList {
  _showSetUpList = showSetUpList;
}

- (void)setSetUpListDisabled:(BOOL)setUpListDisabled {
  if (_setUpListDisabled == setUpListDisabled) {
    return;
  }
  _setUpListDisabled = setUpListDisabled;
  _setUpListToggle.on = !_setUpListDisabled;
}

- (void)setSafetyCheckDisabled:(BOOL)safetyCheckDisabled {
  if (_safetyCheckDisabled == safetyCheckDisabled) {
    return;
  }
  _safetyCheckDisabled = safetyCheckDisabled;
  _safetyCheckToggle.on = !_safetyCheckDisabled;
}

- (void)setTabResumptionDisabled:(BOOL)tabResumptionDisabled {
  if (_tabResumptionDisabled == tabResumptionDisabled) {
    return;
  }
  _tabResumptionDisabled = tabResumptionDisabled;
  _tabResumptionToggle.on = !_tabResumptionDisabled;
}

- (void)setParcelTrackingDisabled:(BOOL)parcelTrackingDisabled {
  if (_parcelTrackingDisabled == parcelTrackingDisabled) {
    return;
  }
  _parcelTrackingDisabled = parcelTrackingDisabled;
  _parcelTrackingToggle.on = !_parcelTrackingDisabled;
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierOptions];

  if (_showSetUpList) {
    NSString* listSymbolName = kListBulletClipboardSymbol;
    _setUpListToggle =
        [self switchItemWithType:ItemTypeToggleSetUpList
                           title:content_suggestions::SetUpListTitleString()
                          symbol:DefaultSymbolWithPointSize(listSymbolName,
                                                            kIconPointSize)];
    _setUpListToggle.on = !_setUpListDisabled;
    [self.tableViewModel addItem:_setUpListToggle
         toSectionWithIdentifier:SectionIdentifierOptions];
  }
  if (IsSafetyCheckMagicStackEnabled()) {
    _safetyCheckToggle = [self
        switchItemWithType:ItemTypeToggleSafetyCheck
                     title:l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_TITLE)
                    symbol:DefaultSymbolWithPointSize(kCheckmarkShieldSymbol,
                                                      kIconPointSize)];
    _safetyCheckToggle.on = !_safetyCheckDisabled;
    [self.tableViewModel addItem:_safetyCheckToggle
         toSectionWithIdentifier:SectionIdentifierOptions];
  }
  if (IsTabResumptionEnabled()) {
    NSString* listSymbolName = kMacbookAndIPhoneSymbol;
    _tabResumptionToggle = [self
        switchItemWithType:ItemTypeToggleTabResumption
                     title:l10n_util::GetNSString(IDS_IOS_TAB_RESUMPTION_TITLE)
                    symbol:DefaultSymbolWithPointSize(listSymbolName,
                                                      kIconPointSize)];
    _tabResumptionToggle.on = !_tabResumptionDisabled;
    [self.tableViewModel addItem:_tabResumptionToggle
         toSectionWithIdentifier:SectionIdentifierOptions];
  }
  if (IsIOSParcelTrackingEnabled()) {
    _parcelTrackingToggle = [self
        switchItemWithType:ItemTypeToggleParcelTracking
                     title:
                         l10n_util::GetNSString(
                             IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_TITLE)
                    symbol:DefaultSymbolWithPointSize(kShippingBoxSymbol,
                                                      kIconPointSize)];
    _parcelTrackingToggle.on = !_parcelTrackingDisabled;
    [self.tableViewModel addItem:_parcelTrackingToggle
         toSectionWithIdentifier:SectionIdentifierOptions];
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  TableViewSwitchCell* switchCell =
      base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
  switch (itemType) {
    case ItemTypeToggleSetUpList:
      [switchCell.switchView addTarget:self
                                action:@selector(setUpListEnabledChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    case ItemTypeToggleSafetyCheck:
      [switchCell.switchView addTarget:self
                                action:@selector(safetyCheckEnabledChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    case ItemTypeToggleTabResumption:
      [switchCell.switchView addTarget:self
                                action:@selector(tabResumptionEnabledChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    case ItemTypeToggleParcelTracking:
      [switchCell.switchView addTarget:self
                                action:@selector(parcelTrackingEnabledChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
  }
  return cell;
}

#pragma mark - Private

- (TableViewSwitchItem*)switchItemWithType:(NSInteger)type
                                     title:(NSString*)title
                                    symbol:(UIImage*)symbol {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:type];
  switchItem.text = title;
  switchItem.iconImage = symbol;
  switchItem.iconTintColor = [UIColor colorNamed:kSolidBlackColor];
  switchItem.accessibilityIdentifier = title;
  return switchItem;
}

- (void)setUpListEnabledChanged:(UISwitch*)switchView {
  [self.modelDelegate setUpListEnabledChanged:switchView.isOn];
}

- (void)safetyCheckEnabledChanged:(UISwitch*)switchView {
  [self.modelDelegate safetyCheckEnabledChanged:switchView.isOn];
}

- (void)tabResumptionEnabledChanged:(UISwitch*)switchView {
  [self.modelDelegate tabResumptionEnabledChanged:switchView.isOn];
}

- (void)parcelTrackingEnabledChanged:(UISwitch*)switchView {
  [self.modelDelegate parcelTrackingEnabledChanged:switchView.isOn];
}

@end
