// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_COORDINATOR_H_

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_presentation_environment.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// LensOverlayCoordinator presents the public interface for the Lens Overlay.
@interface LensOverlayCoordinator : ChromeCoordinator

// Lens overlay view controller.
@property(nonatomic, readonly) UIViewController* viewController;

// The context in which the overlay is presented.
//
// This property should not be changed while the overlay is being presented.
@property(nonatomic, weak) id<LensOverlayPresentationEnvironment>
    presentationEnvironment;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_COORDINATOR_H_
