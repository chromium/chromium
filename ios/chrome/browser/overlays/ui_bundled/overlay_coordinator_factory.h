// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_COORDINATOR_FACTORY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_COORDINATOR_FACTORY_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/overlays/model/public/overlay_modality.h"

class Browser;
class OverlayRequestCoordinatorDelegate;
@class OverlayRequestCoordinator;
class OverlayRequest;

// Factory object provided to OverlayContainerCoordinators that supply overlay
// coordinators for a request.
@interface OverlayRequestCoordinatorFactory : NSObject

// Designated initializer.
- (instancetype)initWithBrowser:(Browser*)browser
                       modality:(OverlayModality)modality
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Returns whether the OverlayRequestCoordinator subclass responsible for
// showing `request`'s overlay UI uses a child UIViewController instead of a
// presented UIViewController.
- (BOOL)coordinatorForRequestUsesChildViewController:(OverlayRequest*)request;

// Creates a coordinator to show `request`'s overlay UI.
- (OverlayRequestCoordinator*)
    newCoordinatorForRequest:(OverlayRequest*)request
                    delegate:(OverlayRequestCoordinatorDelegate*)delegate
          baseViewController:(UIViewController*)baseViewController;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_COORDINATOR_FACTORY_H_
