// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACTOR_OVERLAY_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACTOR_OVERLAY_COMMANDS_H_

#import <Foundation/Foundation.h>

// Commands to show/hide the Actor Overlay UI.
@protocol ActorOverlayCommands <NSObject>

// Shows or updates the Actor Overlay UI with both the border glow and a
// touch-blocking scrim over the content area.
- (void)showActorOverlayWithScrim;

// Shows or updates the Actor Overlay UI with only the border glow, leaving
// page interactions enabled.
- (void)showActorOverlayWithoutScrim;

// Hides the Actor Overlay UI.
- (void)hideActorOverlay;

// Shows a snackbar explaining that interaction with the tab is limited while an
// actor controls it. No-op when the actor overlay scrim is not shown.
- (void)showActorInteractionLimitedSnackbar;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACTOR_OVERLAY_COMMANDS_H_
