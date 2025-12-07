// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_INTERACTIVE_LENS_UI_LENS_OVERLAY_PROMO_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_INTERACTIVE_LENS_UI_LENS_OVERLAY_PROMO_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class LensOverlayPromoContainerViewController;

// Delegate for the LensOverlayPromoContainerViewController.
@protocol LensOverlayPromoContainerViewControllerDelegate <NSObject>

// Called when the user begins interacting with the Lens view.
- (void)lensOverlayPromoContainerViewControllerDidBeginInteraction:
    (LensOverlayPromoContainerViewController*)viewController;

// Called when the user finishes interacting with the Lens view.
- (void)lensOverlayPromoContainerViewControllerDidEndInteraction:
    (LensOverlayPromoContainerViewController*)viewController;

@end

// A view controller that displays the Lens view for the interactive Lens promo.
@interface LensOverlayPromoContainerViewController : UIViewController

// Delegate for the view controller.
@property(nonatomic, weak) id<LensOverlayPromoContainerViewControllerDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_INTERACTIVE_LENS_UI_LENS_OVERLAY_PROMO_CONTAINER_VIEW_CONTROLLER_H_
