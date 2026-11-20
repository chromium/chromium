// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACTOR_OVERLAY_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACTOR_OVERLAY_COMMANDS_H_

#import <Foundation/Foundation.h>

namespace web {
class WebState;
}  // namespace web

// Commands to show/hide the Actor Overlay UI.
@protocol ActorOverlayCommands <NSObject>

// Shows or updates the Actor Overlay UI for the given `webState`. When
// `showScrim` is `YES`, a touch-blocking scrim is shown over the content area
// in addition to the border glow; when `NO`, only the border glow is shown and
// page interactions remain enabled.
- (void)showActorOverlayForWebState:(web::WebState*)webState
                          showScrim:(BOOL)showScrim;

// Hides the Actor Overlay UI.
- (void)hideActorOverlay;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACTOR_OVERLAY_COMMANDS_H_
