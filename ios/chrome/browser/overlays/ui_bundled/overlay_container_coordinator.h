// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_CONTAINER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_CONTAINER_COORDINATOR_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/overlays/model/public/overlay_modality.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator that manages the container view in which overlay UI is displayed.
// The coordinator's view controller should be used to display overlay UI
// implemented using child UIViewControllers.
@interface OverlayContainerCoordinator : ChromeCoordinator

// Initializer for an overlay container that presents overlay for `browser` at
// `modality`.
// TODO(crbug.com/40120484): This is not marked as NS_DESIGNATED_INITIALIZER to
// facilitate the creation of OverlayContainerCoordinators for
// OverlayModality::kTesting.  Annotate as NS_DESIGNATED_INITIALIZER once
// OverlayModality is converted from an enum type.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  modality:(OverlayModality)modality;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The view controller whose presentation context is used to present overlays
// for the OverlayPresenter corresponding with the container's Browser and
// OverlayModality.
@property(nonatomic, readonly) UIViewController* viewController;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_CONTAINER_COORDINATOR_H_
