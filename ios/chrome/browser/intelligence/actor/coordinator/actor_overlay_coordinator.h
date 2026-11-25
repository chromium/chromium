// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTOR_OVERLAY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTOR_OVERLAY_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

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

// Presents a snackbar explaining that interaction with the tab is limited while
// an actor controls it. No-op before `start`, after `stop`, when the scrim is
// hidden, or while the snackbar is already visible.
- (void)showInteractionLimitedSnackbar;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTOR_OVERLAY_COORDINATOR_H_
