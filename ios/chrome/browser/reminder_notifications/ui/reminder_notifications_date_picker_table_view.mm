// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/ui/reminder_notifications_date_picker_table_view.h"

#import "base/check.h"
#import "base/time/time.h"
#import "components/send_tab_to_self/features.h"
#import "ios/chrome/browser/reminder_notifications/ui/constants.h"
#import "ios/chrome/browser/reminder_notifications/ui/reminder_notifications_date_picker_interaction_handler.h"
#import "ios/chrome/browser/reminder_notifications/ui/reminder_notifications_date_picker_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Table cell reuse identifiers
NSString* const kDatePickerCellReuseID = @"DatePickerCell";
NSString* const kTimePickerCellReuseID = @"TimePickerCell";

// Table view section identifiers
NSString* const kDateTimePickerAccessibilityID =
    @"ReminderNotificationsDateTimePicker";

// Row type enum
typedef NS_ENUM(NSInteger, ReminderNotificationsDatePickerRowType) {
  ReminderNotificationsDatePickerRowTypeDate = 0,
  ReminderNotificationsDatePickerRowTypeTime = 1
};

}  // namespace

@interface ReminderNotificationsDatePickerTableView () <UITableViewDataSource,
                                                        UITableViewDelegate>
@end

@implementation ReminderNotificationsDatePickerTableView {
  __weak id<ReminderNotificationsDatePickerInteractionHandler>
      _interactionHandler;
  NSDateFormatter* _dateFormatter;
  NSDateFormatter* _timeFormatter;
}

- (instancetype)initWithInteractionHandler:
    (id<ReminderNotificationsDatePickerInteractionHandler>)interactionHandler {
  if ((self = [super initWithFrame:CGRectZero style:ChromeTableViewStyle()])) {
    _date = [NSDate
        dateWithTimeIntervalSinceNow:
            send_tab_to_self::GetReminderNotificationsDefaultTimeOffset()
                .InSecondsF()];
    _interactionHandler = interactionHandler;

    // Initialize date & time formatters
    _dateFormatter = [[NSDateFormatter alloc] init];
    [_dateFormatter setDateStyle:NSDateFormatterMediumStyle];
    [_dateFormatter setTimeStyle:NSDateFormatterNoStyle];

    _timeFormatter = [[NSDateFormatter alloc] init];
    [_timeFormatter setDateStyle:NSDateFormatterNoStyle];
    [_timeFormatter setTimeStyle:NSDateFormatterShortStyle];

    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.dataSource = self;
    self.delegate = self;
    self.scrollEnabled = NO;
    self.separatorInset =
        UIEdgeInsetsMake(0, kReminderNotificationsTableSeparatorInset, 0, 0);
    self.backgroundColor = [UIColor clearColor];
    self.accessibilityIdentifier = kDateTimePickerAccessibilityID;

    // Remove extra space from UITableViewWrapperView.
    self.directionalLayoutMargins =
        NSDirectionalEdgeInsetsMake(0, CGFLOAT_MIN, 0, CGFLOAT_MIN);
    self.tableHeaderView =
        [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
    self.tableFooterView =
        [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];

    // Register cell classes
    [self registerClass:[ReminderNotificationsDatePickerTableViewCell class]
        forCellReuseIdentifier:kDatePickerCellReuseID];
    [self registerClass:[ReminderNotificationsDatePickerTableViewCell class]
        forCellReuseIdentifier:kTimePickerCellReuseID];
  }
  return self;
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return 2;  // Date and Time rows
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  ReminderNotificationsDatePickerTableViewCell* cell;

  switch ((ReminderNotificationsDatePickerRowType)indexPath.row) {
    case ReminderNotificationsDatePickerRowTypeDate: {
      // Date row
      cell = [tableView dequeueReusableCellWithIdentifier:kDatePickerCellReuseID
                                             forIndexPath:indexPath];
      NSString* formattedDate = [_dateFormatter stringFromDate:_date];
      [cell configureWithLabel:l10n_util::GetNSString(
                                   IDS_IOS_REMINDER_NOTIFICATIONS_DATE_LABEL)
                         value:formattedDate];
      break;
    }
    case ReminderNotificationsDatePickerRowTypeTime: {
      // Time row
      cell = [tableView dequeueReusableCellWithIdentifier:kTimePickerCellReuseID
                                             forIndexPath:indexPath];
      NSString* formattedTime = [_timeFormatter stringFromDate:_date];
      [cell configureWithLabel:l10n_util::GetNSString(
                                   IDS_IOS_REMINDER_NOTIFICATIONS_TIME_LABEL)
                         value:formattedTime];
      break;
    }
  }

  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];

  UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];

  UIDatePickerMode mode;
  switch ((ReminderNotificationsDatePickerRowType)indexPath.row) {
    case ReminderNotificationsDatePickerRowTypeDate:
      mode = UIDatePickerModeDate;
      break;
    case ReminderNotificationsDatePickerRowTypeTime:
      mode = UIDatePickerModeTime;
      break;
  }

  [_interactionHandler showDatePickerFromOriginView:cell
                                 withDatePickerMode:mode];
}

@end
