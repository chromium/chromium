// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/bandwidth/dataplan_usage_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/bandwidth/dataplan_usage_table_view_controller+Testing.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "components/prefs/pref_member.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/prerender/model/prerender_pref.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

using prerender_prefs::NetworkPredictionSetting;

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierOptions = kSectionIdentifierEnumZero,
};

// Item types to enumerate the table items.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeOptionsEnabledAlways = kItemTypeEnumZero,
  ItemTypeOptionsWifiOnly,
  ItemTypeOptionsNever,
};

// Converts an NetworkPredictionSetting, to a corresponding ItemType.
ItemType ItemTypeWithSetting(NetworkPredictionSetting setting) {
  switch (setting) {
    case NetworkPredictionSetting::kEnabledWifiAndCellular: {
      return ItemTypeOptionsEnabledAlways;
    }
    case NetworkPredictionSetting::kEnabledWifiOnly: {
      return ItemTypeOptionsWifiOnly;
    }
    case NetworkPredictionSetting::kDisabled: {
      return ItemTypeOptionsNever;
    }
  }
}

// Converts an ItemType, to a corresponding NetworkPredictionSetting.
NetworkPredictionSetting SettingWithItemType(ItemType item_type) {
  switch (item_type) {
    case ItemTypeOptionsEnabledAlways: {
      return NetworkPredictionSetting::kEnabledWifiAndCellular;
    }
    case ItemTypeOptionsWifiOnly: {
      return NetworkPredictionSetting::kEnabledWifiOnly;
    }
    case ItemTypeOptionsNever: {
      return NetworkPredictionSetting::kDisabled;
    }
  }
}

}  // namespace.

@interface DataplanUsageTableViewController () {
  IntegerPrefMember _settingPreference;
}

@end

@implementation DataplanUsageTableViewController

#pragma mark - Initialization

- (instancetype)initWithPrefs:(PrefService*)prefs
                  settingPref:(const char*)settingPreference
                        title:(NSString*)title {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = title;
    _settingPreference.Init(settingPreference, prefs);
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.estimatedRowHeight = 70;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  self.styler.cellTitleColor = [UIColor colorNamed:kTextPrimaryColor];

  TableViewModel<TableViewItem*>* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierOptions];

  TableViewDetailTextItem* always = [[TableViewDetailTextItem alloc]
      initWithType:ItemTypeWithSetting(
                       NetworkPredictionSetting::kEnabledWifiAndCellular)];
  [always setText:l10n_util::GetNSString(IDS_IOS_OPTIONS_DATA_USAGE_ALWAYS)];
  [always setAccessibilityTraits:UIAccessibilityTraitButton];
  [model addItem:always toSectionWithIdentifier:SectionIdentifierOptions];

  TableViewDetailTextItem* wifi = [[TableViewDetailTextItem alloc]
      initWithType:ItemTypeWithSetting(
                       NetworkPredictionSetting::kEnabledWifiOnly)];
  [wifi setText:l10n_util::GetNSString(IDS_IOS_OPTIONS_DATA_USAGE_ONLY_WIFI)];
  [wifi setAccessibilityTraits:UIAccessibilityTraitButton];
  [model addItem:wifi toSectionWithIdentifier:SectionIdentifierOptions];

  TableViewDetailTextItem* never = [[TableViewDetailTextItem alloc]
      initWithType:ItemTypeWithSetting(NetworkPredictionSetting::kDisabled)];
  [never setText:l10n_util::GetNSString(IDS_IOS_OPTIONS_DATA_USAGE_NEVER)];
  [never setAccessibilityTraits:UIAccessibilityTraitButton];
  [model addItem:never toSectionWithIdentifier:SectionIdentifierOptions];

  [self updateCheckedState];
}

// Updates the checked state of the cells to match the preferences.
- (void)updateCheckedState {
  NetworkPredictionSetting setting =
      static_cast<NetworkPredictionSetting>(_settingPreference.GetValue());

  TableViewModel<TableViewItem*>* model = self.tableViewModel;

  NSMutableArray* modifiedItems = [NSMutableArray array];
  for (TableViewDetailTextItem* item in
       [model itemsInSectionWithIdentifier:SectionIdentifierOptions]) {
    NetworkPredictionSetting itemSetting =
        SettingWithItemType(static_cast<ItemType>(item.type));

    UITableViewCellAccessoryType desiredType =
        itemSetting == setting ? UITableViewCellAccessoryCheckmark
                               : UITableViewCellAccessoryNone;
    if (item.accessoryType != desiredType) {
      item.accessoryType = desiredType;
      [modifiedItems addObject:item];
    }
  }

  [self reconfigureCellsForItems:modifiedItems];
}

#pragma mark - Internal methods

+ (NSString*)currentLabelForPreference:(PrefService*)prefs
                           settingPref:(const char*)settingPreference {
  if (!prefs)
    return nil;

  NetworkPredictionSetting setting = static_cast<NetworkPredictionSetting>(
      prefs->GetInteger(settingPreference));
  switch (setting) {
    case NetworkPredictionSetting::kDisabled: {
      return l10n_util::GetNSString(IDS_IOS_OPTIONS_DATA_USAGE_NEVER);
    }
    case NetworkPredictionSetting::kEnabledWifiOnly: {
      return l10n_util::GetNSString(IDS_IOS_OPTIONS_DATA_USAGE_ONLY_WIFI);
    }

    case NetworkPredictionSetting::kEnabledWifiAndCellular: {
      return l10n_util::GetNSString(IDS_IOS_OPTIONS_DATA_USAGE_ALWAYS);
    }
  }
}

- (void)updateSetting:(NetworkPredictionSetting)newSetting {
  _settingPreference.SetValue(static_cast<int>(newSetting));
  [self updateCheckedState];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NetworkPredictionSetting chosenSetting =
      SettingWithItemType(static_cast<ItemType>(
          [self.tableViewModel itemTypeForIndexPath:indexPath]));
  [self updateSetting:chosenSetting];

  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

@end
