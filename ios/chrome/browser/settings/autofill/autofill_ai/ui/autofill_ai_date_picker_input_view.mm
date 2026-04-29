// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_date_picker_input_view.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const CGFloat kNavigationBarTopPadding = 21;
const CGFloat kIOS26NavigationBarTopVerticalPadding = 0;
const CGFloat kNavigationBarHeight = 44;
const CGFloat kDatePickerTopVerticalPadding = -8;

// Returns the constant to apply to the navigation bar top constraint.
CGFloat NavigationBarTopConstraintConstant() {
  // No top padding should be applied only for iOS 26.0 - iOS 26.3.
  if (@available(iOS 26.0, *)) {
    if (!@available(iOS 26.4, *)) {
      return kIOS26NavigationBarTopVerticalPadding;
    }
  }

  return kNavigationBarTopPadding;
}

}  // namespace

@implementation AutofillAIDatePickerInputView {
  NSLayoutConstraint* _topConstraint;
}

- (instancetype)initWithDate:(NSDate*)date
                       title:(NSString*)title
                      target:(id)target
                  dateAction:(SEL)dateAction
           clearButtonAction:(SEL)clearButtonAction
            doneButtonAction:(SEL)doneButtonAction {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;
    UIDatePicker* datePicker = [self createDatePickerViewWithDate:date
                                                           target:target
                                                           action:dateAction];
    UINavigationBar* navigationBar =
        [self createNavigationBarWithTitle:title
                                    target:target
                         clearButtonAction:clearButtonAction
                          doneButtonAction:doneButtonAction];

    [NSLayoutConstraint activateConstraints:@[
      [datePicker.topAnchor
          constraintEqualToAnchor:navigationBar.bottomAnchor
                         constant:kDatePickerTopVerticalPadding],
    ]];
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToWindow:(UIWindow*)newWindow {
  [super willMoveToWindow:newWindow];
  [self updateTopConstraintWithWindow:newWindow];
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateTopConstraintWithWindow:self.window];
}

#pragma mark - Private

// Updates the navigation bar's top constraint constant based on the device
// orientation.
- (void)updateTopConstraintWithWindow:(UIWindow*)window {
  if (!_topConstraint) {
    return;
  }

  _topConstraint.constant = IsLandscape(window)
                                ? kNavigationBarTopPadding
                                : NavigationBarTopConstraintConstant();
}

// Creates the date picker for the custom input view.
- (UIDatePicker*)createDatePickerViewWithDate:(NSDate*)date
                                       target:(id)target
                                       action:(SEL)action {
  UIDatePicker* datePicker = [[UIDatePicker alloc] init];
  datePicker.datePickerMode = UIDatePickerModeDate;
  datePicker.preferredDatePickerStyle = UIDatePickerStyleWheels;
  datePicker.translatesAutoresizingMaskIntoConstraints = NO;
  [datePicker addTarget:target
                 action:action
       forControlEvents:UIControlEventValueChanged];

  if (date) {
    datePicker.date = date;
  }

  [self addSubview:datePicker];

  AddSameConstraintsToSides(
      datePicker, self,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);

  return datePicker;
}

// Creates the navigation bar's Clear button for the custom input view.
- (UIBarButtonItem*)createClearButtonWithTarget:(id)target action:(SEL)action {
  UIBarButtonItem* clearButton = [[UIBarButtonItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_SETTINGS_DATE_PICKER_CLEAR)
              style:UIBarButtonItemStylePlain
             target:target
             action:action];
  NSDictionary* attributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kRedColor]};
  [clearButton setTitleTextAttributes:attributes forState:UIControlStateNormal];
  return clearButton;
}

// Creates the navigation bar's Done button for the custom input view.
- (UIBarButtonItem*)createDoneButtonWithTarget:(id)target action:(SEL)action {
  return [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:target
                           action:action];
}

// Creates the navigation bar for the custom input view, containing a title, a
// Clear button and a Done button.
- (UINavigationBar*)createNavigationBarWithTitle:(NSString*)title
                                          target:(id)target
                               clearButtonAction:(SEL)clearButtonAction
                                doneButtonAction:(SEL)doneButtonAction {
  UINavigationItem* navigationItem =
      [[UINavigationItem alloc] initWithTitle:title];
  navigationItem.leftBarButtonItem =
      [self createClearButtonWithTarget:target action:clearButtonAction];
  navigationItem.rightBarButtonItem =
      [self createDoneButtonWithTarget:target action:doneButtonAction];

  UINavigationBar* navigationBar = [[UINavigationBar alloc] init];
  navigationBar.translatesAutoresizingMaskIntoConstraints = NO;
  navigationBar.items = @[ navigationItem ];

  [self addSubview:navigationBar];

  AddSameConstraintsToSides(navigationBar, self,
                            LayoutSides::kLeading | LayoutSides::kTrailing);

  NSLayoutConstraint* topConstraint = [navigationBar.topAnchor
      constraintEqualToAnchor:self.topAnchor
                     constant:IsLandscape(self.window)
                                  ? NavigationBarTopConstraintConstant()
                                  : kNavigationBarTopPadding];
  _topConstraint = topConstraint;

  [NSLayoutConstraint activateConstraints:@[
    topConstraint,
    [navigationBar.heightAnchor constraintEqualToConstant:kNavigationBarHeight],
  ]];

  return navigationBar;
}

@end
