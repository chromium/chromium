// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/date_picker_content_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/date_picker_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The maximum width of the date picker or the label in the Autofill AI Entity
// Edit View. This is used to prevent it occupying more space than needed in the
// table view cell.
const CGFloat kAutofillAIEntityEditDateMaxWidth = 150.0;

}  // namespace

@implementation DatePickerContentView {
  // The configuration for this view.
  DatePickerContentConfiguration* _configuration;

  // The compact date picker.
  UIDatePicker* _picker;
}

- (instancetype)initWithConfiguration:
    (DatePickerContentConfiguration*)configuration {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _picker = [[UIDatePicker alloc] init];
    _picker.datePickerMode = UIDatePickerModeDate;
    _picker.preferredDatePickerStyle = UIDatePickerStyleCompact;
    _picker.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_picker];

    _configuration = [configuration copy];
    [self applyConfiguration];
    AddSameConstraints(_picker, self);
    NSLayoutConstraint* maxWidthConstraint = [_picker.widthAnchor
        constraintLessThanOrEqualToConstant:kAutofillAIEntityEditDateMaxWidth];
    maxWidthConstraint.priority = UILayoutPriorityRequired - 1;
    maxWidthConstraint.active = YES;
  }
  return self;
}

#pragma mark - ChromeContentView

- (BOOL)hasCustomAccessibilityActivationPoint {
  return YES;
}

#pragma mark - UIContentView

- (id<UIContentConfiguration>)configuration {
  return _configuration;
}

- (void)setConfiguration:(id<UIContentConfiguration>)configuration {
  _configuration = [base::apple::ObjCCastStrict<DatePickerContentConfiguration>(
      configuration) copy];
  [self applyConfiguration];
}

- (BOOL)supportsConfiguration:(id<UIContentConfiguration>)configuration {
  return [configuration isMemberOfClass:DatePickerContentConfiguration.class];
}

#pragma mark - UIAccessibility

- (CGPoint)accessibilityActivationPoint {
  CGRect frameInScreenCoordinates =
      UIAccessibilityConvertFrameToScreenCoordinates(_picker.bounds, _picker);
  return CGPointMake(CGRectGetMidX(frameInScreenCoordinates),
                     CGRectGetMidY(frameInScreenCoordinates));
}

#pragma mark - Private

- (void)applyConfiguration {
  [_picker removeTarget:nil
                 action:NULL
       forControlEvents:UIControlEventAllEvents];
  [_picker addTarget:_configuration.target
                action:_configuration.selector
      forControlEvents:UIControlEventValueChanged];
  _picker.date = _configuration.date ?: [NSDate date];
  _picker.tintColor = [UIColor colorNamed:kBlue600Color];
  _picker.userInteractionEnabled = _configuration.userInteractionEnabled;
}

@end
