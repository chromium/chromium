// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_TAB_PICKER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_TAB_PICKER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_tab_picker_consumer.h"

@class BaseGridViewController;
@protocol AimPrototypeTabPickerMutator;

// The tab picker view controller for AIM.
@interface AimPrototypeTabPickerViewController
    : UIViewController <AimPrototypeTabPickerConsumer>

/// The embedded grid view controller.
@property(nonatomic, readonly) BaseGridViewController* gridViewController;

/// The tab's picker mutator.
@property(nonatomic, weak) id<AimPrototypeTabPickerMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_TAB_PICKER_VIEW_CONTROLLER_H_
