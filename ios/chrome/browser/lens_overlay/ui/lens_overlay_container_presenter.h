// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONTAINER_PRESENTER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONTAINER_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_presentation_type.h"

@class LensOverlayContainerViewController;
@class SceneState;
@protocol LensOverlayContainerPresenterDelegate;

// Presenter for the Lens overlay container.
@interface LensOverlayContainerPresenter : NSObject

// Delegate for overlay container presentation events.
@property(nonatomic, weak) id<LensOverlayContainerPresenterDelegate> delegate;

// Whether the overlay is presented or not.
@property(nonatomic, readonly, getter=isLensOverlayVisible)
    BOOL lensOverlayVisible;

// Creates a new container presenter instance.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                   containerViewController:(LensOverlayContainerViewController*)
                                               containerViewController;

// Presents the container over the base view controller.
- (void)presentContainerAnimated:(BOOL)animated
                      sceneState:(SceneState*)sceneState
                      completion:(void (^)(void))completion;

// Dismisses the container if presented.
- (void)dismissContainerAnimated:(BOOL)animated
                      completion:(void (^)(void))completion;

// Animates fading the selection UI of the container view controller.
- (void)fadeSelectionUIWithCompletion:(void (^)())completion;

@end

// Presentation delegate for the Lens Overlay container.
@protocol LensOverlayContainerPresenterDelegate

// Notifies the delegate that the container presentation is about to start.
- (void)lensOverlayContainerPresenterWillBeginPresentation:
    (LensOverlayContainerPresenter*)containerPresenter;

// Notifies the delegate that the container presentation has completed.
- (void)lensOverlayContainerPresenterDidCompletePresentation:
            (LensOverlayContainerPresenter*)containerPresenter
                                                    animated:(BOOL)animated;

// Notifies the delegate that the container presentation is about to be
// dismissed.
- (void)lensOverlayContainerPresenterWillDismissPresentation:
    (LensOverlayContainerPresenter*)containerPresenter;

// Informs the delegate that the container presentation was readjusted.
- (void)lensOverlayContainerPresenterDidReadjustPresentation:
    (LensOverlayContainerPresenter*)containerPresenter;

// Returns the required directional edge insets for the presentation.
- (NSDirectionalEdgeInsets)lensOverlayContainerPresenterInsetsForPresentation:
    (LensOverlayContainerPresenter*)containerPresenter;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONTAINER_PRESENTER_H_
