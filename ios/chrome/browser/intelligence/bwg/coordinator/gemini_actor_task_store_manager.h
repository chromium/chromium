// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_ACTOR_TASK_STORE_MANAGER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_ACTOR_TASK_STORE_MANAGER_H_

#import <Foundation/Foundation.h>

@class GeminiActorTaskStore;
@class GeminiActorTaskStoreManager;

namespace actor {
class ActorService;
}  // namespace actor

// Observer of the `GeminiActorTaskStore` instances managed for Gemini actor
// tasks.
@protocol GeminiActorTaskStoreManagerObserver <NSObject>

// Called when a Gemini task starts, with the `store` created for it. Replaces
// and disconnects any active store without calling `didDisconnectStore:`.
- (void)actorTaskStoreManager:(GeminiActorTaskStoreManager*)manager
               didCreateStore:(GeminiActorTaskStore*)store;

// Called when the active Gemini task stops, right after `store` is
// disconnected. `store` must not be used for further processing and any
// reference to it should be released. Not called when `manager` disconnects.
- (void)actorTaskStoreManager:(GeminiActorTaskStoreManager*)manager
           didDisconnectStore:(GeminiActorTaskStore*)store;

@end

// Manages a `GeminiActorTaskStore` for each Gemini actor task, and notifies its
// observers.
// TODO(crbug.com/565875367): Only create stores for the tasks of this manager's
// container (e.g. matching the conversation ID), as every manager receives the
// tasks of the whole profile, so several windows create stores for the same
// task.
@interface GeminiActorTaskStoreManager : NSObject

// Designated initializer. Starts observing the tasks of `actorService`, if any.
- (instancetype)initWithActorService:(actor::ActorService*)actorService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Adds `observer`, held weakly.
- (void)addObserver:(id<GeminiActorTaskStoreManagerObserver>)observer;

// Removes `observer`.
- (void)removeObserver:(id<GeminiActorTaskStoreManagerObserver>)observer;

// Stops observing the tasks and disconnects the active store.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_ACTOR_TASK_STORE_MANAGER_H_
