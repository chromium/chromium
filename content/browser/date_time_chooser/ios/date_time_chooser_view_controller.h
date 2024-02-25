// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_VIEW_CONTROLLER_H_
#define CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_VIEW_CONTROLLER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "content/browser/date_time_chooser/ios/date_time_chooser_coordinator.h"
#import "content/browser/date_time_chooser/ios/date_time_chooser_delegate.h"

// The ViewController that has UIDatePicker.
@interface DateTimeChooserViewController
    : UIViewController <UIPopoverPresentationControllerDelegate>

// Delegate used to tell DateTimeChooser the controller status.
@property(nonatomic, weak) id<DateTimeChooserDelegate> delegate;

// Initializer.
- (instancetype)initWithConfigs:(DateTimeDialogValuePtr)configs;

@end

#endif  // CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_VIEW_CONTROLLER_H_
