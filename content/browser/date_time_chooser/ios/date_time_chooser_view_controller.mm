// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/date_time_chooser/ios/date_time_chooser_view_controller.h"

#import "base/notreached.h"
#import "third_party/blink/public/mojom/choosers/date_time_chooser.mojom.h"
#import "ui/base/ime/text_input_type.h"

const CGFloat kToolBarHeight = 44;

@interface DateTimeChooserViewController ()
// The type to set the date picker mode
@property(nonatomic, assign) ui::TextInputType type;
// Initalized time for the date picker
@property(nonatomic, assign) NSInteger initTimeInMS;
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
  _initTimeInMS = configs->dialog_value;
  return self;
}

- (UIDatePicker*)createUIDatePicker {
  UIDatePicker* datePicker = [[UIDatePicker alloc] init];
  switch (self.type) {
    case ui::TextInputType::TEXT_INPUT_TYPE_DATE:
      datePicker.datePickerMode = UIDatePickerModeDate;
      break;
    case ui::TextInputType::TEXT_INPUT_TYPE_DATE_TIME:
    case ui::TextInputType::TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
    case ui::TextInputType::TEXT_INPUT_TYPE_MONTH:
    case ui::TextInputType::TEXT_INPUT_TYPE_TIME:
    case ui::TextInputType::TEXT_INPUT_TYPE_WEEK:
      // TODO(crbug.com/1461947): Set the mode based on each type and handle the
      // selected value with the format matched to the mode.
      datePicker.datePickerMode = UIDatePickerModeDate;
      break;
    default:
      NOTREACHED() << "Invalid type for a DateTimeChooser.";
      break;
  }
  datePicker.preferredDatePickerStyle = UIDatePickerStyleInline;

  // Convert milliseconds to seconds.
  NSTimeInterval dialogValue = self.initTimeInMS / 1000;
  NSDate* initValue = [NSDate dateWithTimeIntervalSince1970:dialogValue];
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
                        withDate:self.selectedDate];
}

- (void)doneButtonTapped {
  [[self presentingViewController] dismissViewControllerAnimated:YES
                                                      completion:nil];
  // Convert seconds to miliseconds.
  [self.delegate dateTimeChooser:self
            didCloseSuccessfully:TRUE
                        withDate:self.selectedDate];
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
