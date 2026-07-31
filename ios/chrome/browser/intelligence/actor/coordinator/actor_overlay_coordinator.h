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

// `ActorOverlayCoordinator` is a short-lived coordinator launched when a
// tab is actively being actuated by the actor. It manages the actor overlay
// components over the base view controller.
//
// There is only one `ActorOverlayCoordinator` per `Browser`.
@interface ActorOverlayCoordinator : ChromeCoordinator

// Initializes the coordinator with a base view controller, a browser instance,
// the web state undergoing actuation, and the overlay color.
//
// `viewController` is the base view controller used to present the actuation
// UI.
// `browser` is the browser instance containing the tab.
// `webState` is the web state of the tab being actuated.
// `overlayColor` is the color to use for the overlay.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState
                              overlayColor:(UIColor*)overlayColor
    NS_DESIGNATED_INITIALIZER;

// Initializes the coordinator with a base `viewController`, a `browser`
// instance, and the `webState` undergoing actuation, defaulting `overlayColor`
// to `kBlueColor`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTOR_OVERLAY_COORDINATOR_H_
