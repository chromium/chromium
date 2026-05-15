// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_SNACKBAR_ACTOR_TASK_UPDATES_OBSERVER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_SNACKBAR_ACTOR_TASK_UPDATES_OBSERVER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"

class ProfileIOS;

// TODO(crbug.com/512521102): Remove this temporary observer implementation once
// native UI for actor tasks is implemented.
//
// An observer implementation that
// shows a Snackbar message when an active ActorTask's state changes or is
// executing a tool.
@interface SnackbarActorTaskUpdatesObserver
    : NSObject <ActorTaskUpdatesObserver>

// Initializes the observer with the profile.
- (instancetype)initWithProfile:(ProfileIOS*)profile NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_MODEL_SNACKBAR_ACTOR_TASK_UPDATES_OBSERVER_H_
