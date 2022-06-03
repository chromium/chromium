// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_PRESENTATION_CONTEXT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_PRESENTATION_CONTEXT_COORDINATOR_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/overlays/public/overlay_modality.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

class OverlayPresentationContextImpl;

// Coordinator whose presentation context is used for overlay presentation.
// Manages a UIViewController that is presented over the overlay container's
// UIViewController presentation context.  This is necessary due to UIKit's
// custom UIViewController presentation implementation.  The only way to
// present non-modally with UIModalPresentationStyleCustom is by using a custom
// UIPresentationController whose |shouldPresentInFullscreen| property is NO.
// When such a presentation controller is provided, UIKit traverses the view
// hierarchy to find the nearest presented UIViewController.  By presenting a
// UIViewController over a child UIViewController whose
// |definesPresentationContext| property is YES, this coordinator inserts a
// UIKit presentation context upon which custom presentation can occur.
@interface OverlayPresentationContextCoordinator : ChromeCoordinator

// Initializer for an overlay container that presents overlay for |browser| at
// |modality|.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                       presentationContext:
                           (OverlayPresentationContextImpl*)context
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The view controller whose presentation context is used to present overlays.
@property(nonatomic, readonly) UIViewController* viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_OVERLAY_PRESENTATION_CONTEXT_COORDINATOR_H_
