// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_REQUEST_COORDINATOR_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_REQUEST_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class OverlayRequestSupport;
class OverlayRequestCoordinatorDelegate;
class OverlayRequest;

// Coordinator superclass used to present UI for an OverlayRequest.
@interface OverlayRequestCoordinator : ChromeCoordinator

// Returns the request support for this coordinator.  Must return a non-null
// value.
@property(class, nonatomic, readonly)
    const OverlayRequestSupport* requestSupport;

// Returns whether this overlay coordinator type uses child view controllers
// instead of presentating over the container context.  Default value is NO.
@property(class, nonatomic, readonly) BOOL showsOverlayUsingChildViewController;

// Initializer for a coordinator for `request`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                   request:(OverlayRequest*)request
                                  delegate:(OverlayRequestCoordinatorDelegate*)
                                               delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The OverlayRequestCoordinatorDelegate passed on initialization.  Used to
// communicate when the overlay UI is finished being presented and dismissed.
// Overlay UI presentation and dismissal may occur after `-start` and `-stop`,
// even if the overlay is stopped without animation.
@property(nonatomic, assign) OverlayRequestCoordinatorDelegate* delegate;

// The request used to configure the overlay UI.
@property(nonatomic, readonly) OverlayRequest* request;

// The view controller that displays the UI for `request`.
@property(nonatomic, readonly) UIViewController* viewController;

// OverlayRequestCoordinator's `-start` and `-stop` need to support versions
// both with and without animation, as hidden overlays should be shown without
// animation for subsequent presentations.
- (void)startAnimated:(BOOL)animated;
- (void)stopAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_REQUEST_COORDINATOR_H_
