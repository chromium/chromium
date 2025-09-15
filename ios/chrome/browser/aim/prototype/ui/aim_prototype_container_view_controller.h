// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// View Controller that contains the AIM prototype, presenting it modally.
@interface AIMPrototypeContainerViewController : UIViewController

// Adds the input view controller to this ViewController.
- (void)addInputViewController:(UIViewController*)inputViewController;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_CONTAINER_VIEW_CONTROLLER_H_
