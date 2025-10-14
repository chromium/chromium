// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_navigation_consumer.h"
#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_presenter.h"

@class AIMPrototypeComposeboxViewController;
@class AIMPrototypeContainerViewController;

// Delegate for the container view controller.
@protocol AIMPrototypeContainerViewControllerDelegate
- (void)aimPrototypeContainerViewControllerDidTapCloseButton:
    (AIMPrototypeContainerViewController*)viewController;
@end

// View Controller that contains the AIM prototype, presenting it modally.
@interface AIMPrototypeContainerViewController
    : UIViewController <AIMPrototypeNavigationConsumer,
                        OmniboxPopupPresenterDelegate>

// The delegate.
@property(nonatomic, weak) id<AIMPrototypeContainerViewControllerDelegate>
    delegate;

// Adds the input view controller to this ViewController.
- (void)addInputViewController:
    (AIMPrototypeComposeboxViewController*)inputViewController;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_PROTOTYPE_CONTAINER_VIEW_CONTROLLER_H_
