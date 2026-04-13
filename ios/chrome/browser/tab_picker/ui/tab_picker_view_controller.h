// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_PICKER_UI_TAB_PICKER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TAB_PICKER_UI_TAB_PICKER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/tab_picker/ui/tab_picker_consumer.h"

@class BaseGridViewController;
@protocol TabPickerCommands;
@protocol TabPickerMutator;

// The tab picker view controller.
@interface TabPickerViewController : UIViewController <TabPickerConsumer>

/// The embedded grid view controller.
@property(nonatomic, readonly) BaseGridViewController* gridViewController;

/// The tab's picker mutator.
@property(nonatomic, weak) id<TabPickerMutator> mutator;

/// The handler for TabPickerCommands.
@property(nonatomic, weak) id<TabPickerCommands> tabPickerHandler;

@end

#endif  // IOS_CHROME_BROWSER_TAB_PICKER_UI_TAB_PICKER_VIEW_CONTROLLER_H_
