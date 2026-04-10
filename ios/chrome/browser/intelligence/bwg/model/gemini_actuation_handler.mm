// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_handler.h"

#import <vector>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"

@implementation GeminiActuationHandler {
  // The ActorService to use for actuating tasks.
  raw_ptr<actor::ActorService> _actorService;
}

- (instancetype)initWithActorService:(actor::ActorService*)actorService {
  self = [super init];
  if (self) {
    _actorService = actorService;
  }
  return self;
}

#pragma mark - GeminiActuationDelegate

- (actor::ActorTaskId)createTaskWithTitle:(NSString*)title {
  // TODO(crbug.com/496163970): Implement and test.
  return actor::ActorTaskId();
}

- (void)addTaskUpdatesObserver:(id<ActorTaskUpdatesObserver>)observer
                     forTaskID:(actor::ActorTaskId)taskID {
  // TODO(crbug.com/496163970): Implement and test.
}

- (void)setTaskInterventionDelegate:(id<ActorTaskInterventionDelegate>)delegate
                          forTaskID:(actor::ActorTaskId)taskID {
  // TODO(crbug.com/496163970): Implement and test.
}

- (void)performActionsWithTaskID:(actor::ActorTaskId)taskID
                      taskUpdate:(NSString*)taskUpdate
          serializedActionProtos:(NSArray<NSData*>*)serializedActionProtos
                      completion:
                          (void (^)(BOOL success,
                                    std::vector<bool> results))completionBlock {
  // TODO(crbug.com/496163970): Implement and test.
}

- (void)pauseTaskWithID:(actor::ActorTaskId)taskID {
  // TODO(crbug.com/496163970): Implement and test.
}

- (void)stopTaskWithID:(actor::ActorTaskId)taskID
                reason:(actor::ActorTaskStoppedReason)reason {
  // TODO(crbug.com/496163970): Implement and test.
}

@end
