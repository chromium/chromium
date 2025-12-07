// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_INTERACTIVE_LENS_UI_INTERACTIVE_LENS_OVERLAY_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_INTERACTIVE_LENS_UI_INTERACTIVE_LENS_OVERLAY_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/first_run/ui_bundled/interactive_lens/ui/lens_interactive_promo_results_page_presenter.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

// Delegate for actions on the Interactive Lens screen.
@protocol InteractiveLensPromoDelegate <NSObject>

// Called when the user tapped on the "continue" button. `interaction` is YES if
// the user has interacted with the Lens view of the screen.
- (void)didTapContinueButtonWithInteraction:(BOOL)interaction;

@end

// View controller for the Interactive Lens screen in the First Run Experience.
@interface InteractiveLensOverlayPromoViewController
    : UIViewController <LensInteractivePromoResultsPagePresenterDelegate>

// Delegate for actions on the Interactive Lens screen.
@property(nonatomic, weak) id<InteractiveLensPromoDelegate> delegate;

// The view controller that contains the Lens UI.
@property(nonatomic, readonly) UIViewController* lensContainerViewController;

// The image to be displayed in the promo.
@property(nonatomic, readonly) UIImage* lensSearchImage;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_INTERACTIVE_LENS_UI_INTERACTIVE_LENS_OVERLAY_PROMO_VIEW_CONTROLLER_H_
