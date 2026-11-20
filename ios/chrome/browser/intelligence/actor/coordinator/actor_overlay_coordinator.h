// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTOR_OVERLAY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTOR_OVERLAY_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace web {
class WebState;
}  // namespace web

class Browser;

// `ActorOverlayCoordinator` is a short-lived coordinator launched when the
// active tab is part of an active actor task (either actor-controlled or
// user-controlled). It manages the actor overlay components over the base view
// controller.
//
// There is only one `ActorOverlayCoordinator` per `Browser`.
@interface ActorOverlayCoordinator : ChromeCoordinator

// Whether the touch-blocking scrim view is visible. Defaults to `YES`. When
// `NO`, only the border glow is shown, page interactions remain enabled, and
// any active interaction-limited snackbar is dismissed.
@property(nonatomic, assign, getter=isScrimVisible) BOOL scrimVisible;

// Initializes the coordinator with a base view controller, a browser instance,
// the active tab's web state, the scrim color, and the glow color.
//
// `viewController` is the base view controller used to present the overlay UI.
// `browser` is the browser instance containing the tab.
// `webState` is the web state of the active tab in the actor task.
// `scrimColor` is the color to use for the scrim view.
// `glowColor` is the color to use for the border glow view.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState
                                scrimColor:(UIColor*)scrimColor
                                 glowColor:(UIColor*)glowColor
    NS_DESIGNATED_INITIALIZER;

// Initializes the coordinator with a base `viewController`, a `browser`
// instance, and the active tab's `webState`, defaulting the scrim and glow
// colors to blue.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTOR_OVERLAY_COORDINATOR_H_
