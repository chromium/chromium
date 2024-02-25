// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/date_time_chooser/ios/date_time_chooser_mediator.h"

#import "content/browser/date_time_chooser/ios/date_time_chooser_ios.h"
#import "content/browser/date_time_chooser/ios/date_time_chooser_util.h"
#import "ui/base/ime/text_input_type.h"

@interface DateTimeChooserMediator ()
@property(nonatomic, assign) content::DateTimeChooserIOS* dateTimeChooser;
@end

@implementation DateTimeChooserMediator
- (instancetype)initWithDateTimeChooser:
    (content::DateTimeChooserIOS*)dateTimeChooser {
  if (!(self = [super init])) {
    return nil;
  }

  _dateTimeChooser = dateTimeChooser;
  return self;
}

#pragma mark - DateTimeChooserDelegate

- (void)dateTimeChooser:(DateTimeChooserViewController*)chooser
    didCloseSuccessfully:(BOOL)success
                withDate:(NSDate*)date
                 forType:(ui::TextInputType)type {
  double selected_value = 0.0f;
  if (type == ui::TextInputType::TEXT_INPUT_TYPE_MONTH) {
    selected_value = static_cast<double>(GetNumberOfMonthsFromDate(date));
  } else {
    selected_value = [date timeIntervalSince1970] * 1000.f;
  }
  self.dateTimeChooser->OnDialogClosed(success, selected_value);
}

@end
