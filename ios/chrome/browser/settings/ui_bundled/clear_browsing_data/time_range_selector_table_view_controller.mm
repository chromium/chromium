// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/time_range_selector_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/prefs/pref_member.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/time_range_selector_table_view_controller+Testing.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierOptions = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypePastHour = kItemTypeEnumZero,
  ItemTypePastDay,
  ItemTypePastWeek,
  ItemTypeLastFourWeeks,
  ItemTypeBeginningOfTime,
};

// Returns ItemType from the browsing_data::TimePeriod value stored as
// an int in PrefService.
std::optional<ItemType> ItemTypeFromTimePeriodAsInt(int time_period_as_int) {
  switch (time_period_as_int) {
    case base::to_underlying(browsing_data::TimePeriod::LAST_HOUR):
      return ItemTypePastHour;

    case base::to_underlying(browsing_data::TimePeriod::LAST_DAY):
      return ItemTypePastDay;

    case base::to_underlying(browsing_data::TimePeriod::LAST_WEEK):
      return ItemTypePastWeek;

    case base::to_underlying(browsing_data::TimePeriod::FOUR_WEEKS):
      return ItemTypeLastFourWeeks;

    case base::to_underlying(browsing_data::TimePeriod::ALL_TIME):
      return ItemTypeBeginningOfTime;

    default:
      return std::nullopt;
  }
}

// Converts `item_type` into the corresponding browsing_data::TimePeriod
// value.
browsing_data::TimePeriod TimePeriodFromItemType(ItemType item_type) {
  switch (item_type) {
    case ItemTypePastHour:
      return browsing_data::TimePeriod::LAST_HOUR;

    case ItemTypePastDay:
      return browsing_data::TimePeriod::LAST_DAY;

    case ItemTypePastWeek:
      return browsing_data::TimePeriod::LAST_WEEK;

    case ItemTypeLastFourWeeks:
      return browsing_data::TimePeriod::FOUR_WEEKS;

    case ItemTypeBeginningOfTime:
      return browsing_data::TimePeriod::ALL_TIME;
  }

  NOTREACHED();
}

// Returns the string to display for `item_type`.
int StringIdentifierForItemType(ItemType item_type) {
  switch (item_type) {
    case ItemTypePastHour:
      return IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_HOUR;

    case ItemTypePastDay:
      return IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_DAY;

    case ItemTypePastWeek:
      return IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_WEEK;

    case ItemTypeLastFourWeeks:
      return IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_LAST_FOUR_WEEKS;

    case ItemTypeBeginningOfTime:
      return IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_BEGINNING_OF_TIME;
  }

  NOTREACHED();
}

}  // namespace

@interface TimeRangeSelectorTableViewController () {
  IntegerPrefMember _timeRangePref;
}

@end

@implementation TimeRangeSelectorTableViewController

#pragma mark Initialization

- (instancetype)initWithPrefs:(PrefService*)prefs {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    self.title = l10n_util::GetNSString(
        IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TITLE);
    _timeRangePref.Init(browsing_data::prefs::kDeleteTimePeriod, prefs);
    self.shouldHideDoneButton = YES;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierOptions];

  [model addItem:[self timeRangeItemWithOption:ItemTypePastHour]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [model addItem:[self timeRangeItemWithOption:ItemTypePastDay]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [model addItem:[self timeRangeItemWithOption:ItemTypePastWeek]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [model addItem:[self timeRangeItemWithOption:ItemTypeLastFourWeeks]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [model addItem:[self timeRangeItemWithOption:ItemTypeBeginningOfTime]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [self updateCheckedState];
}

- (void)updateCheckedState {
  TableViewModel* model = self.tableViewModel;
  const std::optional<ItemType> wantedType =
      ItemTypeFromTimePeriodAsInt(_timeRangePref.GetValue());

  NSMutableArray* modifiedItems = [NSMutableArray array];
  for (TableViewItem* item in
       [model itemsInSectionWithIdentifier:SectionIdentifierOptions]) {
    const ItemType itemType = static_cast<ItemType>(item.type);
    const UITableViewCellAccessoryType desiredType =
        itemType == wantedType ? UITableViewCellAccessoryCheckmark
                               : UITableViewCellAccessoryNone;

    if (item.accessoryType != desiredType) {
      item.accessoryType = desiredType;
      [modifiedItems addObject:item];
    }
  }

  [self reconfigureCellsForItems:modifiedItems];
}

- (void)updatePrefValue:(browsing_data::TimePeriod)prefValue {
  _timeRangePref.SetValue(base::to_underlying(prefValue));
  [self updateCheckedState];
}

- (TableViewDetailTextItem*)timeRangeItemWithOption:(ItemType)itemOption {
  TableViewDetailTextItem* item =
      [[TableViewDetailTextItem alloc] initWithType:itemOption];
  item.text = l10n_util::GetNSString(StringIdentifierForItemType(itemOption));
  item.accessibilityTraits = UIAccessibilityTraitButton;
  return item;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  const ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);
  [self updatePrefValue:TimePeriodFromItemType(itemType)];
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
}

@end
