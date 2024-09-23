// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_CONTAINER_COORDINATOR_INITIALIZATION_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_CONTAINER_COORDINATOR_INITIALIZATION_H_

#import "ios/chrome/browser/overlays/ui_bundled/overlay_container_coordinator.h"

class OverlayPresentationContextImpl;

// TODO(crbug.com/40120484): This initializer is only necessary to prevent the
// test modality code from getting compiled into releases, and can be removed
// once OverlayModality is converted from an enum to a class.
@interface OverlayContainerCoordinator (Initialization)

// Initializer for a coordinator that manages the base UIViewController for
// overlay UI implemented using child UIViewControllers for `context` at
// `modality`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                       presentationContext:
                           (OverlayPresentationContextImpl*)context;

@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_CONTAINER_COORDINATOR_INITIALIZATION_H_
