// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_TAB_PICKER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_TAB_PICKER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class BaseGridViewController;

// The tab picker view controller for AIM.
@interface AimPrototypeTabPickerViewController : UIViewController

/// The embedded grid view controller.
@property(nonatomic, readonly) BaseGridViewController* gridViewController;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_TAB_PICKER_VIEW_CONTROLLER_H_
