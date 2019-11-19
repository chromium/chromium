// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_REQUEST_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_REQUEST_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

class OverlayRequestCoordinatorDelegate;
class OverlayRequest;

// Coordinator superclass used to present UI for an OverlayRequest.
@interface OverlayRequestCoordinator : ChromeCoordinator

// Returns whether this overlay coordinator type supports |request|.
+ (BOOL)supportsRequest:(OverlayRequest*)request;

// Returns whether this overlay coordinator type uses child view controllers
// instead of presentating over the container context.  Default value is NO.
@property(class, nonatomic, readonly) BOOL showsOverlayUsingChildViewController;

// Initializer for a coordinator for |request|.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                   request:(OverlayRequest*)request
                                  delegate:(OverlayRequestCoordinatorDelegate*)
                                               delegate
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The OverlayRequestCoordinatorDelegate passed on initialization.  Used to
// communicate when the overlay UI is finished being presented and dismissed.
// Overlay UI presentation and dismissal may occur after |-start| and |-stop|,
// even if the overlay is stopped without animation.
@property(nonatomic, readonly) OverlayRequestCoordinatorDelegate* delegate;

// The request used to configure the overlay UI.
@property(nonatomic, readonly) OverlayRequest* request;

// The view controller that displays the UI for |request|.
@property(nonatomic, readonly) UIViewController* viewController;

// OverlayRequestCoordinator's |-start| and |-stop| need to support versions
// both with and without animation, as hidden overlays should be shown without
// animation for subsequent presentations.
- (void)startAnimated:(BOOL)animated;
- (void)stopAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_REQUEST_COORDINATOR_H_
