// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_TAB_PICKER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_TAB_PICKER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/composebox/ui/composebox_tab_picker_consumer.h"

@class BaseGridViewController;
@protocol ComposeboxTabPickerCommands;
@protocol ComposeboxTabPickerMutator;

// The tab picker view controller for AIM.
@interface ComposeboxTabPickerViewController
    : UIViewController <ComposeboxTabPickerConsumer>

/// The embedded grid view controller.
@property(nonatomic, readonly) BaseGridViewController* gridViewController;

/// The tab's picker mutator.
@property(nonatomic, weak) id<ComposeboxTabPickerMutator> mutator;

/// The handler for ComposeboxTabPickerCommands.
@property(nonatomic, weak) id<ComposeboxTabPickerCommands>
    composeboxTabPickerHandler;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_TAB_PICKER_VIEW_CONTROLLER_H_
