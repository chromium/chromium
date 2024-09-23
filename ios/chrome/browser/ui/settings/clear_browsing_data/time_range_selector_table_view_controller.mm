// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/time_range_selector_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/prefs/pref_member.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/time_range_selector_table_view_controller+Testing.h"
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

const int kStringIDS[] = {
    IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_HOUR,
    IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_DAY,
    IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_WEEK,
    IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_LAST_FOUR_WEEKS,
    IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_BEGINNING_OF_TIME,
    IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_OLDER_THAN_30_DAYS};

const browsing_data::TimePeriod kTimePeriodUnused[]{
    // Last 15 Minutes is not yet available on iOS.
    browsing_data::TimePeriod::LAST_15_MINUTES};

static_assert(
    std::size(kStringIDS) ==
        static_cast<int>(browsing_data::TimePeriod::TIME_PERIOD_LAST) -
            std::size(kTimePeriodUnused) + 1,
    "Strings have to match the enum values.");

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

  [model addItem:[self timeRangeItemWithOption:ItemTypePastHour
                                 textMessageID:kStringIDS[0]]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [model addItem:[self timeRangeItemWithOption:ItemTypePastDay
                                 textMessageID:kStringIDS[1]]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [model addItem:[self timeRangeItemWithOption:ItemTypePastWeek
                                 textMessageID:kStringIDS[2]]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [model addItem:[self timeRangeItemWithOption:ItemTypeLastFourWeeks
                                 textMessageID:kStringIDS[3]]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [model addItem:[self timeRangeItemWithOption:ItemTypeBeginningOfTime
                                 textMessageID:kStringIDS[4]]
      toSectionWithIdentifier:SectionIdentifierOptions];

  [self updateCheckedState];
}

- (void)updateCheckedState {
  int timeRangePrefValue = _timeRangePref.GetValue();
  TableViewModel* model = self.tableViewModel;

  NSMutableArray* modifiedItems = [NSMutableArray array];
  for (TableViewItem* item in
       [model itemsInSectionWithIdentifier:SectionIdentifierOptions]) {
    NSInteger itemPrefValue = item.type - kItemTypeEnumZero;
    UITableViewCellAccessoryType desiredType =
        itemPrefValue == timeRangePrefValue ? UITableViewCellAccessoryCheckmark
                                            : UITableViewCellAccessoryNone;
    if (item.accessoryType != desiredType) {
      item.accessoryType = desiredType;
      [modifiedItems addObject:item];
    }
  }

  [self reconfigureCellsForItems:modifiedItems];
}

- (void)updatePrefValue:(int)prefValue {
  _timeRangePref.SetValue(prefValue);
  [self updateCheckedState];
}

- (TableViewDetailTextItem*)timeRangeItemWithOption:(ItemType)itemOption
                                      textMessageID:(int)textMessageID {
  TableViewDetailTextItem* item =
      [[TableViewDetailTextItem alloc] initWithType:itemOption];
  item.text = l10n_util::GetNSString(textMessageID);
  item.accessibilityTraits = UIAccessibilityTraitButton;
  return item;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  int timePeriod = itemType - kItemTypeEnumZero;
  DCHECK_LE(timePeriod,
            static_cast<int>(browsing_data::TimePeriod::TIME_PERIOD_LAST));
  [self updatePrefValue:timePeriod];
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - Class methods

+ (NSString*)timePeriodLabelForPrefs:(PrefService*)prefs {
  if (!prefs) {
    return nil;
  }
  int prefValue = prefs->GetInteger(browsing_data::prefs::kDeleteTimePeriod);
  if (prefValue < 0 ||
      static_cast<size_t>(prefValue) >= std::size(kStringIDS)) {
    return nil;
  }
  return l10n_util::GetNSString(kStringIDS[prefValue]);
}

@end
