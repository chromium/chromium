// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/ntp/set_up_list_prefs.h"
#import "ios/chrome/browser/ntp_tiles/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_prefs.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
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
};

}  // namespace

@interface MagicStackHalfSheetTableViewController () <BooleanObserver>
@end

@implementation MagicStackHalfSheetTableViewController {
  PrefBackedBoolean* _setUpListDisabled;
  PrefBackedBoolean* _safetyCheckDisabled;
  PrefBackedBoolean* _tabResumptionDisabled;

  TableViewSwitchItem* _setUpListToggle;
  TableViewSwitchItem* _safetyCheckToggle;
  TableViewSwitchItem* _tabResumptionToggle;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  UITableViewStyle style = ChromeTableViewStyle();
  if (IsIOSSetUpListEnabled()) {
    _setUpListDisabled = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:set_up_list_prefs::kDisabled];
    [_setUpListDisabled setObserver:self];
  }
  if (IsSafetyCheckMagicStackEnabled()) {
    _safetyCheckDisabled = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:safety_check_prefs::
                                kSafetyCheckInMagicStackDisabledPref];
    [_safetyCheckDisabled setObserver:self];
  }
  if (IsTabResumptionEnabled()) {
    _tabResumptionDisabled = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:tab_resumption_prefs::kTabResumptioDisabledPref];
    [_tabResumptionDisabled setObserver:self];
  }

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

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierOptions];

  if (IsIOSSetUpListEnabled()) {
    NSString* listSymbolName = kListBulletRectangle;
    if (@available(iOS 16.0, *)) {
      listSymbolName = kListBulletClipboard;
    }
    _setUpListToggle = [self
        switchItemWithType:ItemTypeToggleSetUpList
                     title:l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_TITLE)
                    symbol:DefaultSymbolWithPointSize(listSymbolName,
                                                      kIconPointSize)];
    _setUpListToggle.on = !_setUpListDisabled.value;
    [self.tableViewModel addItem:_setUpListToggle
         toSectionWithIdentifier:SectionIdentifierOptions];
  }
  if (IsSafetyCheckMagicStackEnabled()) {
    _safetyCheckToggle = [self
        switchItemWithType:ItemTypeToggleSafetyCheck
                     title:l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_TITLE)
                    symbol:DefaultSymbolWithPointSize(kCheckmarkShield,
                                                      kIconPointSize)];
    _safetyCheckToggle.on = !_safetyCheckDisabled.value;
    [self.tableViewModel addItem:_safetyCheckToggle
         toSectionWithIdentifier:SectionIdentifierOptions];
  }
  if (IsTabResumptionEnabled()) {
    _tabResumptionToggle = [self
        switchItemWithType:ItemTypeToggleTabResumption
                     title:l10n_util::GetNSString(IDS_IOS_TAB_RESUMPTION_TITLE)
                    symbol:DefaultSymbolWithPointSize(kMacbookAndIPhone,
                                                      kIconPointSize)];
    _tabResumptionToggle.on = !_tabResumptionDisabled.value;
    [self.tableViewModel addItem:_tabResumptionToggle
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
  }
  return cell;
}

#pragma mark - Boolean Observer

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _setUpListDisabled) {
    _setUpListToggle.on = !_setUpListDisabled.value;
  } else if (observableBoolean == _safetyCheckDisabled) {
    _safetyCheckToggle.on = !_safetyCheckDisabled.value;
  } else if (observableBoolean == _tabResumptionDisabled) {
    _tabResumptionToggle.on = !_tabResumptionDisabled.value;
  }
}

#pragma mark - Private

- (TableViewSwitchItem*)switchItemWithType:(NSInteger)type
                                     title:(NSString*)title
                                    symbol:(UIImage*)symbol {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:type];
  switchItem.text = title;
  switchItem.iconImage = symbol;
  switchItem.iconTintColor = UIColor.blackColor;
  switchItem.accessibilityIdentifier = title;
  return switchItem;
}

- (void)setUpListEnabledChanged:(UISwitch*)switchView {
  [_setUpListDisabled setValue:!switchView.isOn];
}

- (void)safetyCheckEnabledChanged:(UISwitch*)switchView {
  [_safetyCheckDisabled setValue:!switchView.isOn];
}

- (void)tabResumptionEnabledChanged:(UISwitch*)switchView {
  [_tabResumptionDisabled setValue:!switchView.isOn];
}

@end
