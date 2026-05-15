// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/snackbar_actor_task_updates_observer.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_utils.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"

namespace {

// Task state strings.
NSString* const kStateStarted = @"Started";
NSString* const kStateActing = @"Acting";
NSString* const kStateReflecting = @"Reflecting";
NSString* const kStatePaused = @"Paused";
NSString* const kStateCancelled = @"Cancelled";
NSString* const kStateFinished = @"Finished";
NSString* const kStateWaitingForUser = @"Waiting for user";
NSString* const kStateFailed = @"Failed";

// Default task strings.
NSString* const kDefaultTaskTitle = @"Actor Task";
NSString* const kDefaultTaskSubtitle = @"No task";

// Format strings.
NSString* const kStateFormat = @"State: %@";
NSString* const kExecutingFormat = @"Executing: %@";

// Returns the user-facing string for `state` if a transition to it should be
// displayed, or nil otherwise.
NSString* DisplayedStateStringForActorTaskState(actor::ActorTaskState state) {
  switch (state) {
    case actor::ActorTaskState::kInit:
      return kStateStarted;
    case actor::ActorTaskState::kReflecting:
      return kStateReflecting;
    case actor::ActorTaskState::kCancelled:
      return kStateCancelled;
    case actor::ActorTaskState::kFinished:
      return kStateFinished;
    case actor::ActorTaskState::kWaitingOnUser:
      return kStateWaitingForUser;
    case actor::ActorTaskState::kFailed:
      return kStateFailed;
    case actor::ActorTaskState::kPausedByUser:
      return kStatePaused;
    case actor::ActorTaskState::kActing:
    case actor::ActorTaskState::kPausedByActor:
      return nil;
  }
}

}  // namespace

@implementation SnackbarActorTaskUpdatesObserver {
  // Command dispatcher handler for showing snackbars.
  __weak id<SnackbarCommands> _snackbarCommands;
  // The title of the actor task.
  NSString* _taskTitle;
  // The latest update string describing the task execution.
  NSString* _lastTaskUpdate;
}

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  self = [super init];
  if (self) {
    if (profile) {
      BrowserList* browserList = BrowserListFactory::GetForProfile(profile);
      if (browserList) {
        Browser* browser =
            browser_list_utils::GetMostActiveSceneBrowser(browserList);
        if (browser) {
          CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
          if ([dispatcher dispatchingForProtocol:@protocol(SnackbarCommands)]) {
            _snackbarCommands =
                HandlerForProtocol(dispatcher, SnackbarCommands);
          }
        }
      }
    }
  }
  return self;
}

#pragma mark - Private

// Shows a snackbar message configured with the current task title, last task
// update, and the given `leafSubtitle`.
- (void)showSnackbarWithLeafSubtitle:(NSString*)leafSubtitle {
  if (_snackbarCommands) {
    NSString* titleText =
        _taskTitle.length > 0 ? _taskTitle : kDefaultTaskTitle;
    NSString* subtitleText =
        _lastTaskUpdate.length > 0 ? _lastTaskUpdate : kDefaultTaskSubtitle;

    SnackbarMessage* message =
        [[SnackbarMessage alloc] initWithTitle:titleText];
    message.subtitle = subtitleText;
    message.secondarySubtitle = leafSubtitle;

    [_snackbarCommands showSnackbarMessage:message];
  }
}

#pragma mark - ActorTaskUpdatesObserver

- (void)didRegisterAsObserverForTaskID:(actor::ActorTaskId)taskID
                             taskTitle:(NSString*)taskTitle
                            taskUpdate:(NSString*)taskUpdate
                          currentState:(actor::ActorTaskState)state
                             webStates:(NSArray<NSNumber*>*)webStatesIDs {
  _taskTitle = [taskTitle copy];
  _lastTaskUpdate = [taskUpdate copy];

  NSString* stateDescription = DisplayedStateStringForActorTaskState(state);
  if (stateDescription) {
    NSString* leafText =
        [NSString stringWithFormat:kStateFormat, stateDescription];
    [self showSnackbarWithLeafSubtitle:leafText];
  }
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
         didChangeState:(actor::ActorTaskState)newState
              fromState:(actor::ActorTaskState)oldState {
  NSString* stateDescription = DisplayedStateStringForActorTaskState(newState);
  if (stateDescription) {
    NSString* leafText =
        [NSString stringWithFormat:kStateFormat, stateDescription];
    [self showSnackbarWithLeafSubtitle:leafText];
  }
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
        willExecuteTool:(NSString*)toolString
             taskUpdate:(NSString*)taskUpdate
             onWebState:(web::WebStateID)webStateID {
  _lastTaskUpdate = [taskUpdate copy];
  NSString* leafText = [NSString stringWithFormat:kExecutingFormat, toolString];
  [self showSnackbarWithLeafSubtitle:leafText];
}

- (void)actorTaskDidStopWithID:(actor::ActorTaskId)taskID
                    finalState:(actor::ActorTaskState)finalState {
  // No specific snackbar message required for stopped unless transitioning
  // state.
}

@end
