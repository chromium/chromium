// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_actor_task_store_manager.h"

#import "base/check.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/memory/raw_ptr.h"
#import "components/actor/core/task_source_info.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_lifecycle_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_actor_task_store.h"

@interface GeminiActorTaskStoreManager () <ActorTaskLifecycleObserver>
@end

@implementation GeminiActorTaskStoreManager {
  // Null once disconnected.
  raw_ptr<actor::ActorService> _actorService;
  // Observers of the managed stores.
  CRBProtocolObservers<GeminiActorTaskStoreManagerObserver>* _observers;
  // Store of the running Gemini task, if any.
  // TODO(crbug.com/565875367): Track multiple concurrent tasks.
  GeminiActorTaskStore* _activeStore;
}

- (instancetype)initWithActorService:(actor::ActorService*)actorService {
  self = [super init];
  if (self) {
    _observers = static_cast<
        CRBProtocolObservers<GeminiActorTaskStoreManagerObserver>*>(
        [CRBProtocolObservers
            observersWithProtocol:@protocol(
                                      GeminiActorTaskStoreManagerObserver)]);
    _actorService = actorService;
    if (_actorService) {
      _actorService->AddTaskLifecycleObserver(self);
    }
  }
  return self;
}

- (void)addObserver:(id<GeminiActorTaskStoreManagerObserver>)observer {
  CHECK(observer);
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<GeminiActorTaskStoreManagerObserver>)observer {
  [_observers removeObserver:observer];
}

- (void)disconnect {
  if (_actorService) {
    _actorService->RemoveTaskLifecycleObserver(self);
    _actorService = nullptr;
  }
  [_activeStore disconnect];
  _activeStore = nil;
}

#pragma mark - ActorTaskLifecycleObserver

- (void)actorServiceDidStartTaskWithID:(actor::ActorTaskId)taskID
                            sourceInfo:
                                (const actor::TaskSourceInfo&)sourceInfo {
  if (sourceInfo.type != actor::TaskSourceInfo::Client::kGlic) {
    return;
  }
  [_activeStore disconnect];
  _activeStore = [[GeminiActorTaskStore alloc] initWithTaskID:taskID
                                                 actorService:_actorService];
  [_observers actorTaskStoreManager:self didCreateStore:_activeStore];
}

- (void)actorServiceDidStopTaskWithID:(actor::ActorTaskId)taskID
                           sourceInfo:(const actor::TaskSourceInfo&)sourceInfo
                           finalState:(actor::ActorTaskState)finalState {
  if (!_activeStore || _activeStore.taskID != taskID) {
    return;
  }
  GeminiActorTaskStore* store = _activeStore;
  _activeStore = nil;
  [store disconnect];
  [_observers actorTaskStoreManager:self didDisconnectStore:store];
}

@end
