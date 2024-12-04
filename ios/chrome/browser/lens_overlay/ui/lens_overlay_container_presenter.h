// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONTAINER_PRESENTER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONTAINER_PRESENTER_H_

#import <UIKit/UIKit.h>

@class LensOverlayContainerViewController;
@class SceneState;

// Presenter for the Lens overlay container.
@interface LensOverlayContainerPresenter : NSObject

// Whether the overlay is presented or not;
@property(nonatomic, readonly) BOOL isLensOverlayVisible;

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

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONTAINER_PRESENTER_H_
