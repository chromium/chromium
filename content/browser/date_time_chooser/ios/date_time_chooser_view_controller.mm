// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/date_time_chooser/ios/date_time_chooser_view_controller.h"

#import "base/notreached.h"
#import "content/browser/date_time_chooser/ios/date_time_chooser_util.h"
#import "third_party/blink/public/mojom/choosers/date_time_chooser.mojom.h"
#import "ui/base/ime/text_input_type.h"

const CGFloat kToolBarHeight = 44;

@interface DateTimeChooserViewController ()
// The type to set the date picker mode
@property(nonatomic, assign) ui::TextInputType type;
// Initalized time for the date picker. For TEXT_INPUT_TYPE_MONTH, it is the
// number of month. Otherwise, it's in milliseconds.
@property(nonatomic, assign) NSInteger initTime;
// Updated with the selected date in the date picker
@property(nonatomic, assign) NSDate* selectedDate;
@end

@implementation DateTimeChooserViewController

- (instancetype)initWithConfigs:(DateTimeDialogValuePtr)configs {
  if (!(self = [super init])) {
    return nil;
  }
  _delegate = nil;
  _type = configs->dialog_type;
  _initTime = configs->dialog_value;
  // TODO(crbug.com/40274667): Handle other values in `configs` such as minimum
  // or maximum.
  return self;
}

- (UIDatePicker*)createUIDatePicker {
  UIDatePicker* datePicker = [[UIDatePicker alloc] init];
  UIDatePickerMode mode = UIDatePickerModeDate;
  UIDatePickerStyle style = UIDatePickerStyleAutomatic;
  NSDate* initValue;
  switch (self.type) {
    case ui::TextInputType::TEXT_INPUT_TYPE_DATE:
      initValue = [NSDate dateWithTimeIntervalSince1970:self.initTime / 1000];
      mode = UIDatePickerModeDate;
      style = UIDatePickerStyleInline;
      break;
    case ui::TextInputType::TEXT_INPUT_TYPE_TIME:
      initValue = [NSDate dateWithTimeIntervalSince1970:self.initTime / 1000];
      mode = UIDatePickerModeTime;
      style = UIDatePickerStyleWheels;
      break;
    case ui::TextInputType::TEXT_INPUT_TYPE_DATE_TIME:
    case ui::TextInputType::TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
      initValue = [NSDate dateWithTimeIntervalSince1970:self.initTime / 1000];
      mode = UIDatePickerModeDateAndTime;
      style = UIDatePickerStyleInline;
      break;
    case ui::TextInputType::TEXT_INPUT_TYPE_MONTH:
      initValue = GetDateFromNumberOfMonths(self.initTime);
      mode = UIDatePickerModeDate;
      style = UIDatePickerStyleWheels;
      break;
    case ui::TextInputType::TEXT_INPUT_TYPE_WEEK:
      initValue = [NSDate dateWithTimeIntervalSince1970:self.initTime / 1000];
      // UIDatePicker doesn't have a mode for the week number. So, it opens
      // UIDatePicker with UIDatePickerModeDate and converts the selected
      // to the week number.
      mode = UIDatePickerModeDate;
      style = UIDatePickerStyleInline;
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid type for a DateTimeChooser.";
      break;
  }

  datePicker.timeZone = [NSTimeZone timeZoneWithAbbreviation:@"UTC"];
  datePicker.datePickerMode = mode;
  datePicker.preferredDatePickerStyle = style;

  [datePicker setDate:initValue animated:FALSE];
  [datePicker addTarget:self
                 action:@selector(datePickerValueChanged:)
       forControlEvents:UIControlEventValueChanged];
  return datePicker;
}

- (void)cancelButtonTapped {
  [[self presentingViewController] dismissViewControllerAnimated:YES
                                                      completion:nil];
  [self.delegate dateTimeChooser:self
            didCloseSuccessfully:FALSE
                        withDate:self.selectedDate
                         forType:self.type];
}

- (void)doneButtonTapped {
  [[self presentingViewController] dismissViewControllerAnimated:YES
                                                      completion:nil];
  // Convert seconds to miliseconds.
  [self.delegate dateTimeChooser:self
            didCloseSuccessfully:TRUE
                        withDate:self.selectedDate
                         forType:self.type];
}

- (void)datePickerValueChanged:(UIDatePicker*)datePicker {
  self.selectedDate = datePicker.date;
}

// Adds subviews for a UI component with UIDatePicker and buttons.
//
// -------------------------------------
//
//
//              UIDatePicker
//
//
// --------------------------------------
// | Cancel  |  flexibleSpace  |  Done  |
// --------------------------------------
//
- (void)viewWillAppear:(BOOL)animated {
  UIDatePicker* datePicker = [self createUIDatePicker];
  // Create a ToolBar for buttons.
  UIToolbar* toolbar = [[UIToolbar alloc]
      initWithFrame:CGRectMake(0, 0, datePicker.frame.size.width,
                               kToolBarHeight)];
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonTapped)];
  UIBarButtonItem* flexibleSpace = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(doneButtonTapped)];

  // Add the UIBarButtonItem to the UIToolbar
  [toolbar setItems:@[ cancelButton, flexibleSpace, doneButton ]];

  // Vertical stack view that holds UIDatePicker and buttons.
  UIStackView* verticalStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ datePicker, toolbar ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.spacing = 0;
  verticalStack.distribution = UIStackViewDistributionFill;
  verticalStack.layoutMarginsRelativeArrangement = YES;
  verticalStack.layoutMargins = UIEdgeInsetsMake(0, 0, 0, 0);
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:verticalStack];
  [self.view setBounds:CGRectMake(CGRectGetMinX(datePicker.bounds),
                                  CGRectGetMinY(datePicker.bounds),
                                  CGRectGetWidth(datePicker.bounds),
                                  CGRectGetHeight(datePicker.bounds) +
                                      CGRectGetHeight(toolbar.bounds))];
}

@end
