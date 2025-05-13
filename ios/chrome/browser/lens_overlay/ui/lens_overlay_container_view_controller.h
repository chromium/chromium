// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/public/provider/chrome/browser/lens/lens_overlay_api.h"

@protocol LensOverlayCommands;
@protocol LensOverlayContainerDelegate;

/// The top level view controller for lens overlay.
/// Contains or presents the other view controllers.
/// Displays top-level chrome like close button.
@interface LensOverlayContainerViewController : UIViewController

- (instancetype)initWithLensOverlayCommandsHandler:
    (id<LensOverlayCommands>)handler NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/// The delegate for Lens Overlay Container events.
@property(nonatomic, weak) id<LensOverlayContainerDelegate> delegate;

/// The selection view controller contained by this view controller.
/// Currently should be set by `viewDidLoad` and only set once.
@property(nonatomic, strong)
    UIViewController<ChromeLensOverlay>* selectionViewController;

/// Disables the interaction with the presented overlay.
@property(nonatomic, assign) BOOL selectionInteractionDisabled;

/// Whether the side panel is being presented.
@property(nonatomic, readonly, getter=isSidePanelPresented)
    BOOL sidePanelPresented;

// Animates fading the selection UI of the container.
- (void)fadeSelectionUIWithDuration:(NSTimeInterval)duration
                         completion:(void (^)())completion;

/// Presents the given view controller in a side panel, optionally animated.
- (void)presentViewControllerInSidePanel:(UIViewController*)viewController
                                animated:(BOOL)animated
                              completion:(ProceduralBlock)completion;

/// Dismisses the side panel presentation, optionally animated.
- (void)dismissSidePanelAnimated:(BOOL)animated
                      completion:(ProceduralBlock)completion;

@end

/// The delegate of the lens overlay container.
@protocol LensOverlayContainerDelegate <NSObject>

/// Called after the container was added to a view hierarchy.
- (void)lensOverlayContainerDidAppear:(LensOverlayContainerViewController*)
                                          lensOverlayContainerViewController
                             animated:(BOOL)animated;

/// Called when the container changes the current horizontal size class
- (void)lensOverlayContainerDidChangeSizeClass:
    (LensOverlayContainerViewController*)lensOverlayContainerViewController;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONTAINER_VIEW_CONTROLLER_H_
