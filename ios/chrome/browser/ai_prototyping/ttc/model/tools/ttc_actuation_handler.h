// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_HANDLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_HANDLER_H_

#import <Foundation/Foundation.h>

#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

class ProfileIOS;
class WebStateList;

namespace actor {
class ActorService;
}  // namespace actor

// Bridges TalkToChrome tool execution to the Chromium ActorService pipeline.
// Manages the lifetime and tab bindings of Actor tasks, coordinating browser
// actions executed on behalf of the user.
@interface TTCActuationHandler : NSObject

// Designated initializer with Profile, WebStateList, and Browser ID.
// @param profile The Profile associated with the browser session.
// @param webStateList The WebStateList associated with the browser.
// @param browserID The session ID representing the browser window.
// @return An initialized instance.
- (instancetype)initWithProfile:(ProfileIOS*)profile
                   webStateList:(WebStateList*)webStateList
                      browserID:(SessionID)browserID NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects from the browser and stops all active tasks. Must be called
// before destruction.
- (void)disconnect;

// Creates a new Actor task titled `title` associated with the currently active
// WebState in the browser, and binds that WebState to the task.
// @param title The human-readable title describing the task.
// @return The allocated `ActorTaskId`, or a null `ActorTaskId` if there is no
// active WebState, if the active WebState is off-the-record, or if ActorService
// is unavailable.
- (actor::ActorTaskId)createTaskWithTitle:(NSString*)title;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_HANDLER_H_
