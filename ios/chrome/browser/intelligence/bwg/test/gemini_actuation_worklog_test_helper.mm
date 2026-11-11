// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/test/gemini_actuation_worklog_test_helper.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_coordinator.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_actor_task_store.h"

@implementation GeminiActuationWorklogTestHelper {
  // Store of the observed task. Retained here because the worklog mediator
  // only holds it weakly.
  GeminiActorTaskStore* _taskStore;
}

- (actor::ActorTaskId)currentTaskId {
  return _taskStore ? _taskStore.taskID : actor::ActorTaskId();
}

- (void)startObservingTaskWithID:(actor::ActorTaskId)taskID
                    actorService:(actor::ActorService*)actorService {
  CHECK(self.coordinator);
  CHECK(actorService);

  [_taskStore disconnect];
  _taskStore = [[GeminiActorTaskStore alloc] initWithTaskID:taskID
                                               actorService:actorService];
  [self.coordinator startObservingDataSource:_taskStore
                               actionHandler:_taskStore];
}

- (void)stopObservingTask {
  [self.coordinator stopObservingDataSource];
  [_taskStore disconnect];
  _taskStore = nil;
}

- (void)stop {
  [self stopObservingTask];
  [self.coordinator.baseViewController dismissViewControllerAnimated:NO
                                                          completion:nil];
  [self.coordinator stop];
  self.coordinator = nil;
}

#pragma mark - Properties

- (id<ActorTaskInterventionDelegate>)interventionDelegate {
  return _taskStore;
}

@end
