// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/actor_profile_agent.h"

#import "base/ios/block_types.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/background_task/background_continued_processing_app_agent.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_configuration.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_context.h"
#import "ios/chrome/app/background_task/background_continued_processing_task_request.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_task_background_worker.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

@implementation ActorProfileAgent {
  // The profile's actor service, resolved on first use.
  base::WeakPtr<actor::ActorService> _actorService;
}

#pragma mark - ProfileStateAgent

// Registers as a task provider while attached to a profile state, and forgets
// the previous profile state's service.
- (void)setProfileState:(ProfileState*)profileState {
  [[self backgroundTaskAppAgent] removeTaskProvider:self];
  [super setProfileState:profileState];
  _actorService.reset();
  [[self backgroundTaskAppAgent] addTaskProvider:self];
}

#pragma mark - BackgroundContinuedProcessingTaskProvider

- (NSArray<BackgroundContinuedProcessingTaskRequest*>*)
    continuedProcessingTaskRequests {
  // Without a service, there is no task to keep alive.
  actor::ActorService* service = [self actorService];
  if (!service) {
    return @[];
  }

  NSMutableArray<BackgroundContinuedProcessingTaskRequest*>* requests =
      [NSMutableArray array];
  for (const base::WeakPtr<actor::ActorTaskBackgroundWorker>& worker :
       service->GetWeakTaskBackgroundWorkers()) {
    if (!worker || !worker->ShouldRequestBackgroundTask()) {
      continue;
    }
    [requests addObject:[self requestForBackgroundWorker:worker]];
  }
  return requests;
}

#pragma mark - Private

// Returns the app agent that requests background tasks, if any.
- (BackgroundContinuedProcessingAppAgent*)backgroundTaskAppAgent {
  return [BackgroundContinuedProcessingAppAgent
      agentFromApp:self.profileState.appState];
}

// Returns the profile's actor service if it exists. Never creates the service.
- (actor::ActorService*)actorService {
  if (_actorService) {
    return _actorService.get();
  }

  ProfileIOS* profile = self.profileState.profile;
  if (!profile) {
    return nullptr;
  }

  actor::ActorService* service =
      actor::ActorServiceFactory::GetForProfileIfExists(profile);
  if (service) {
    _actorService = service->GetWeakPtr();
  }

  return service;
}

// Returns the request for `worker`'s task. The started context is handed
// to `worker`, or failed if the task is gone by then.
- (BackgroundContinuedProcessingTaskRequest*)requestForBackgroundWorker:
    (const base::WeakPtr<actor::ActorTaskBackgroundWorker>&)worker {
  base::WeakPtr<actor::ActorTaskBackgroundWorker> weakWorker = worker;
  void (^startedHandler)(BackgroundContinuedProcessingTaskContext*) =
      ^(BackgroundContinuedProcessingTaskContext* context) {
        if (weakWorker) {
          weakWorker->SetContext(context);
        } else {
          // Nobody owns the context, so end it rather than let it expire.
          [context setTaskCompletedWithSuccess:NO];
        }
      };

  NSString* identifier =
      base::SysUTF8ToNSString(base::NumberToString(worker->task_id().value()));
  return [[BackgroundContinuedProcessingTaskRequest alloc]
      initWithIdentifier:identifier
           configuration:[self configurationForBackgroundWorker:worker]
          startedHandler:startedHandler];
}

// Returns the configuration for `worker`'s task: its title, the fail
// submission strategy and an expiration handler that pauses the task.
- (BackgroundContinuedProcessingTaskConfiguration*)
    configurationForBackgroundWorker:
        (const base::WeakPtr<actor::ActorTaskBackgroundWorker>&)worker {
  __weak ActorProfileAgent* weakSelf = self;
  const actor::ActorTaskId taskId = worker->task_id();
  ProceduralBlock expirationHandler = ^{
    [weakSelf backgroundTaskDidExpireForActorTask:taskId];
  };

  BackgroundContinuedProcessingTaskConfiguration* config =
      [[BackgroundContinuedProcessingTaskConfiguration alloc]
              initWithTitle:base::SysUTF8ToNSString(worker->title())
                   subtitle:@""
          expirationHandler:expirationHandler];
  // Fail rather than queue, since running the task at an arbitrary time breaks
  // a lot of assumptions.
  config.submissionStrategy =
      BackgroundContinuedProcessingSubmissionStrategy::kFail;
  return config;
}

// Pauses the actor task identified by `taskId`, as its background task expired.
- (void)backgroundTaskDidExpireForActorTask:(actor::ActorTaskId)taskId {
  if (_actorService) {
    _actorService->PauseTask(taskId, /*from_actor=*/false);
  }
}

@end
