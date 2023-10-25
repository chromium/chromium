// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_DELEGATE_H_
#define CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ui/base/ime/text_input_type.h"

@class DateTimeChooserViewController;

// Delegate to handle actions.
@protocol DateTimeChooserDelegate <NSObject>

// Method invoked when the user closed a dialog.
- (void)dateTimeChooser:(DateTimeChooserViewController*)chooser
    didCloseSuccessfully:(BOOL)success
                withDate:(NSDate*)date
                 forType:(ui::TextInputType)type;
@end

#endif  // CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_DELEGATE_H_
