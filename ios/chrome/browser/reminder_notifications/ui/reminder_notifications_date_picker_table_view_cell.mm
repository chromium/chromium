// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/ui/reminder_notifications_date_picker_table_view_cell.h"

#import "ios/chrome/browser/reminder_notifications/ui/constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

// This cell uses custom labels with manual anchor-based layout instead of
// the built-in content configuration styles because it implements
// a specialized date/time picker UI for Tab Reminders that requires precise
// control over appearance and interaction. The design needs to independently
// manage date and time selection in a way that intentionally differs from
// Chrome iOS's standard table view styling patterns.
@implementation ReminderNotificationsDatePickerTableViewCell {
  UILabel* _titleLabel;
  UILabel* _valueLabel;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.selectionStyle = UITableViewCellSelectionStyleNone;
    self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

    // Create label.
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.textColor = [UIColor colorNamed:kGrey800Color];
    _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _titleLabel.adjustsFontForContentSizeCategory = YES;

    // Create value label.
    _valueLabel = [[UILabel alloc] init];
    _valueLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _valueLabel.textColor = [UIColor colorNamed:kBlueColor];
    _valueLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _valueLabel.adjustsFontForContentSizeCategory = YES;

    [self.contentView addSubview:_titleLabel];
    [self.contentView addSubview:_valueLabel];

    [NSLayoutConstraint activateConstraints:@[
      [_titleLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kReminderNotificationsContentPadding],
      [_titleLabel.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_valueLabel.leadingAnchor
          constraintEqualToAnchor:_titleLabel.trailingAnchor
                         constant:kReminderNotificationsContentPadding],
      [_valueLabel.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor]
    ]];
  }
  return self;
}

- (void)configureWithLabel:(NSString*)labelText value:(NSString*)valueText {
  _titleLabel.text = labelText;
  _valueLabel.text = valueText;

  // Set accessibility properties
  self.accessibilityIdentifier =
      [NSString stringWithFormat:@"ReminderNotifications%@Row", labelText];
}

@end
