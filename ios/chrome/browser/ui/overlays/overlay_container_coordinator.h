// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_CONTAINER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_CONTAINER_COORDINATOR_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/overlays/public/overlay_modality.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

// Coordinator that manages displaying of UI for OverlayRequests.  An instance
// of this coordinator should be created for each Browser at every
// OverlayModality.
@interface OverlayContainerCoordinator : ChromeCoordinator

// Initializer for an overlay container that presents overlay for |browser| at
// |modality|.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  modality:(OverlayModality)modality
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The view controller whose presentation context is used to present overlays
// for the OverlayPresenter corresponding with the container's Browser and
// OverlayModality.
@property(nonatomic, readonly) UIViewController* viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_CONTAINER_COORDINATOR_H_
