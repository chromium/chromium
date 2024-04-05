// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/time_range_selector_table_view_controller.h"

#import <objc/NSObjCRuntime.h>

#import "base/apple/foundation_util.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_consumer.h"
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
  browsing_data::TimePeriod _timePeriod;
}

@end

@implementation TimeRangeSelectorTableViewController

#pragma mark Initialization

- (instancetype)initWithTimePeriod:(browsing_data::TimePeriod)timePeriod {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    self.title = l10n_util::GetNSString(
        IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TITLE);
    _timePeriod = timePeriod;
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
  TableViewModel* model = self.tableViewModel;

  NSMutableArray* modifiedItems = [NSMutableArray array];
  for (TableViewItem* item in
       [model itemsInSectionWithIdentifier:SectionIdentifierOptions]) {
    NSInteger timePeriodType =
        static_cast<NSInteger>(_timePeriod) + kItemTypeEnumZero;
    UITableViewCellAccessoryType desiredType =
        item.type == timePeriodType ? UITableViewCellAccessoryCheckmark
                                    : UITableViewCellAccessoryNone;
    if (item.accessoryType != desiredType) {
      item.accessoryType = desiredType;
      [modifiedItems addObject:item];
    }
  }

  [self reconfigureCellsForItems:modifiedItems];
}

- (void)updateTimePeriod:(browsing_data::TimePeriod)timePeriod {
  if (_timePeriod == timePeriod) {
    return;
  }
  _timePeriod = timePeriod;
  [self.consumer updateTimePeriod:timePeriod];
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
  [self updateTimePeriod:static_cast<browsing_data::TimePeriod>(
                             itemType - kItemTypeEnumZero)];
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - Class methods

+ (NSString*)timePeriodLabel:(browsing_data::TimePeriod)timePeriod {
  int indexTimePeriod = static_cast<int>(timePeriod);

  // Check if the cast returned a valid index inside the bounds of `kStringIDS`.
  if (indexTimePeriod < 0 ||
      static_cast<size_t>(timePeriod) >= std::size(kStringIDS)) {
    return nil;
  }

  return l10n_util::GetNSString(kStringIDS[indexTimePeriod]);
}

@end
