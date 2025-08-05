// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_consumer.h"

@class AIMPrototypeViewController;

// Delegate for the AIM prototype view controller.
@protocol AIMPrototypeViewControllerDelegate
- (void)aimPrototypeViewControllerDidTapCloseButton:
    (AIMPrototypeViewController*)viewController;
- (void)aimPrototypeViewControllerDidTapGalleryButton:
    (AIMPrototypeViewController*)viewController;
@end

// View controller for the AIM prototype.
@interface AIMPrototypeViewController : UIViewController <AIMPrototypeConsumer>

@property(nonatomic, weak) id<AIMPrototypeViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_VIEW_CONTROLLER_H_
