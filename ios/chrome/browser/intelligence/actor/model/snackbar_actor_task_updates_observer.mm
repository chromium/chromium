// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/snackbar_actor_task_updates_observer.h"

#import <optional>
#import <vector>

#import "base/functional/bind.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/utils/actor_tool_utils.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_ui_utils.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_utils.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/web/public/web_state.h"

const CGFloat kGeminiActorSnackbarBottomOffset = 95.0;
const NSInteger kActorOverlayViewTag = 24937;

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

NSString* const kUnknownTool = @"Unknown tool";

// The duration the snackbar should be displayed.
const NSTimeInterval kSnackbarDuration = 4.0;

// The size of the Gemini logo in the snackbar.
const CGFloat kGeminiLogoSize = 18.0;

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

// A helper class containing a task state or tool execution event, responsible
// for determining the corresponding user-facing display string.
@interface SnackbarActorUpdate : NSObject

// Initializes the SnackbarActorUpdate with a state.
- (instancetype)initWithState:(actor::ActorTaskState)state;

// Initializes the SnackbarActorUpdate with a tool.
- (instancetype)initWithTool:(actor::ToolType)tool
                  taskUpdate:(NSString*)taskUpdate;

// Internal initializer for a SnackbarActorUpdate with either a state or a
// tool and taskUpdate.
- (instancetype)initWithState:(std::optional<actor::ActorTaskState>)state
                         tool:(std::optional<actor::ToolType>)tool
                   taskUpdate:(NSString*)taskUpdate NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Returns the user-facing string to be displayed in the snackbar, or nil if
// the SnackbarActorUpdate should not trigger a snackbar.
- (NSString*)displayString;

// The state represented by this SnackbarActorUpdate, if any.
@property(nonatomic, readonly) std::optional<actor::ActorTaskState> state;

// The tool represented by this SnackbarActorUpdate, if any.
@property(nonatomic, readonly) std::optional<actor::ToolType> tool;

// Returns YES if this SnackbarActorUpdate triggers a "reflecting" state
// SnackbarActorUpdate fallback upon completion.
@property(nonatomic, readonly) BOOL triggersReflectingOnDismissal;

// The task update string, if any.
@property(nonatomic, copy, readonly) NSString* taskUpdate;

@end

@implementation SnackbarActorUpdate

- (instancetype)initWithState:(actor::ActorTaskState)state {
  return [self initWithState:state tool:std::nullopt taskUpdate:nil];
}

- (instancetype)initWithTool:(actor::ToolType)tool
                  taskUpdate:(NSString*)taskUpdate {
  return [self initWithState:std::nullopt tool:tool taskUpdate:taskUpdate];
}

- (instancetype)initWithState:(std::optional<actor::ActorTaskState>)state
                         tool:(std::optional<actor::ToolType>)tool
                   taskUpdate:(NSString*)taskUpdate {
  self = [super init];
  if (self) {
    _state = state;
    _tool = tool;
    _taskUpdate = [taskUpdate copy];
  }
  return self;
}

- (NSString*)displayString {
  if (_state) {
    return DisplayedStateStringForActorTaskState(*_state);
  }
  if (_tool) {
    std::optional<std::string> toolDisplayString =
        actor::ToolTypeToToolDisplayString(*_tool);
    return toolDisplayString ? base::SysUTF8ToNSString(*toolDisplayString)
                             : kUnknownTool;
  }
  return nil;
}

- (BOOL)triggersReflectingOnDismissal {
  return !_state || *_state != actor::ActorTaskState::kReflecting;
}

@end

@implementation SnackbarActorTaskUpdatesObserver {
  // A weak pointer to the ActorService.
  base::WeakPtr<actor::ActorService> _actorService;
  // Command dispatcher handler for showing Gemini Actor snackbars.
  __weak id<GeminiActorSnackbarCommands> _geminiSnackbarHandler;
  // The title of the actor task.
  NSString* _taskTitle;
  // Latest shown task update.
  NSString* _latestShownTaskUpdate;
  // The most recently scheduled SnackbarActorUpdate request.
  SnackbarActorUpdate* _latestScheduledSnackbarUpdate;
  // The SnackbarActorUpdate currently presented on screen.
  SnackbarActorUpdate* _activeSnackbarUpdate;
  // The WebState currently being overlaid.
  base::WeakPtr<web::WebState> _webState;
}

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  self = [super init];
  if (self) {
    if (profile) {
      _actorService =
          actor::ActorServiceFactory::GetForProfile(profile)->GetWeakPtr();
      BrowserList* browserList = BrowserListFactory::GetForProfile(profile);
      if (browserList) {
        Browser* browser =
            browser_list_utils::GetMostActiveSceneBrowser(browserList);
        if (browser) {
          CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
          if ([dispatcher
                  dispatchingForProtocol:@protocol(
                                             GeminiActorSnackbarCommands)]) {
            _geminiSnackbarHandler =
                HandlerForProtocol(dispatcher, GeminiActorSnackbarCommands);
          }
        }
      }
    }
  }
  return self;
}

- (void)disconnect {
  _actorService.reset();
  _geminiSnackbarHandler = nil;
}

- (void)dealloc {
  [self hideOverlay];
  [self disconnect];
}

#pragma mark - Private

// Queues a SnackbarActorUpdate by posting it to the SequencedTaskRunner.
- (void)queueSnackbarWithUpdate:(SnackbarActorUpdate*)snackbarUpdate {
  _latestScheduledSnackbarUpdate = snackbarUpdate;
  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf showSnackbarForUpdate:snackbarUpdate];
      }));
}

// Shows a SnackbarActorUpdate for an update on the SequencedTaskRunner.
- (void)showSnackbarForUpdate:(SnackbarActorUpdate*)snackbarUpdate {
  if (!_geminiSnackbarHandler) {
    return;
  }

  // If a newer SnackbarActorUpdate has already been scheduled, skip this older
  // one entirely.
  if (snackbarUpdate != _latestScheduledSnackbarUpdate) {
    return;
  }

  NSString* secondarySubtitle = [snackbarUpdate displayString];
  if (!secondarySubtitle) {
    return;
  }

  // Do not show the "reflecting" state SnackbarActorUpdate if another active
  // update is currently presented on screen. It will be shown upon dismissal of
  // that active update, or ignored if a reflecting snackbar is already showing.
  if (snackbarUpdate.state == actor::ActorTaskState::kReflecting &&
      _activeSnackbarUpdate) {
    return;
  }

  // Populate the snackbar content using available text fields in priority
  // order:
  // 1. Task title (if state is initial).
  // 2. The newest task update.
  // 3. The status or tool display string.
  // This ensures that we are using the topmost SnackbarMessage fields to
  // display the content.
  std::vector<NSString*> snackbarStrings;
  if (snackbarUpdate.state == actor::ActorTaskState::kInit &&
      _taskTitle.length > 0) {
    snackbarStrings.push_back(_taskTitle);
  }
  if (snackbarUpdate.taskUpdate.length > 0 &&
      ![snackbarUpdate.taskUpdate isEqualToString:_latestShownTaskUpdate]) {
    snackbarStrings.push_back(snackbarUpdate.taskUpdate);
    _latestShownTaskUpdate = [snackbarUpdate.taskUpdate copy];
  }
  NSString* displayString = [snackbarUpdate displayString];
  if (displayString.length > 0) {
    snackbarStrings.push_back(displayString);
  }

  if (snackbarStrings.empty()) {
    return;
  }

  SnackbarMessage* message =
      [[SnackbarMessage alloc] initWithTitle:snackbarStrings[0]];
  if (snackbarStrings.size() > 1) {
    message.subtitle = snackbarStrings[1];
  }
  if (snackbarStrings.size() > 2) {
    message.secondarySubtitle = snackbarStrings[2];
  }
  message.duration = kSnackbarDuration;
  message.leadingAccessoryImage =
      [GeminiUIUtils createGradientGeminiLogo:kGeminiLogoSize];

  _activeSnackbarUpdate = snackbarUpdate;
  __weak __typeof(self) weakSelf = self;
  message.completionHandler = ^(BOOL completed) {
    [weakSelf snackbarDismissedWithUpdate:snackbarUpdate];
  };
  [_geminiSnackbarHandler
      showGeminiActorSnackbarMessage:message
              additionalBottomOffset:kGeminiActorSnackbarBottomOffset];
}

// Responds to the dismissal/completion of a presented SnackbarActorUpdate.
- (void)snackbarDismissedWithUpdate:(SnackbarActorUpdate*)snackbarUpdate {
  if (snackbarUpdate == _activeSnackbarUpdate) {
    _activeSnackbarUpdate = nil;
  }

  if (snackbarUpdate.triggersReflectingOnDismissal &&
      _latestScheduledSnackbarUpdate &&
      _latestScheduledSnackbarUpdate.state ==
          actor::ActorTaskState::kReflecting) {
    [self queueSnackbarWithUpdate:
              [[SnackbarActorUpdate alloc]
                  initWithState:actor::ActorTaskState::kReflecting]];
  }
}

// Shows the blue overlay on the specified WebState. It's a 25% opacity overlay
// over the web view which blocks user inputs.
- (void)showOverlayForWebState:(web::WebState*)webState {
  if (!webState) {
    return;
  }

  UIView* webStateView = webState->GetView();
  if (!webStateView) {
    return;
  }

  UIView* existingOverlay = [webStateView viewWithTag:kActorOverlayViewTag];
  if (existingOverlay) {
    return;
  }

  UIView* overlayView = [[UIView alloc] initWithFrame:webStateView.bounds];
  overlayView.backgroundColor = [UIColor colorWithRed:0.0
                                                green:0.478
                                                 blue:1.0
                                                alpha:0.25];
  overlayView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  overlayView.tag = kActorOverlayViewTag;
  overlayView.userInteractionEnabled = YES;

  [webStateView addSubview:overlayView];
  _webState = webState->GetWeakPtr();
}

// Retrieves the WebState using ActorService and shows an overlay on it.
- (void)showOverlayForWebStateID:(web::WebStateID)webStateID
                          taskID:(actor::ActorTaskId)taskID {
  if (!_actorService) {
    return;
  }

  web::WebState* webState = _actorService->GetWebStateForID(webStateID, taskID);
  if (_webState.get() != webState) {
    [self hideOverlay];
  }
  [self showOverlayForWebState:webState];
}

// Hides the overlay from the WebState.
- (void)hideOverlay {
  if (!_webState) {
    return;
  }

  UIView* webStateView = _webState->GetView();
  if (webStateView) {
    UIView* existingOverlay = [webStateView viewWithTag:kActorOverlayViewTag];
    if (existingOverlay) {
      [existingOverlay removeFromSuperview];
    }
  }

  _webState.reset();
}

#pragma mark - ActorTaskUpdatesObserver

- (void)didRegisterAsObserverForTaskID:(actor::ActorTaskId)taskID
                             taskTitle:(NSString*)taskTitle
                            taskUpdate:(NSString*)taskUpdate
                          currentState:(actor::ActorTaskState)state
                             webStates:(NSArray<NSNumber*>*)webStatesIDs {
  _taskTitle = [taskTitle copy];

  [self queueSnackbarWithUpdate:[[SnackbarActorUpdate alloc]
                                    initWithState:state
                                             tool:std::nullopt
                                       taskUpdate:taskUpdate]];

  // Show overlay on the first controlled web state (since we can currently
  // assume there is only one).
  if (webStatesIDs.count > 0) {
    web::WebStateID webStateID =
        web::WebStateID::FromSerializedValue([webStatesIDs[0] intValue]);
    [self showOverlayForWebStateID:webStateID taskID:taskID];
  }
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
         didAddWebState:(web::WebStateID)webStateID {
  [self showOverlayForWebStateID:webStateID taskID:taskID];
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
         didChangeState:(actor::ActorTaskState)newState
              fromState:(actor::ActorTaskState)oldState {
  [self queueSnackbarWithUpdate:[[SnackbarActorUpdate alloc]
                                    initWithState:newState]];

  if (newState == actor::ActorTaskState::kInit && _webState) {
    [self showOverlayForWebStateID:_webState->GetUniqueIdentifier()
                            taskID:taskID];
  }
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
        willExecuteTool:(actor::ToolType)toolType
             taskUpdate:(NSString*)taskUpdate
             onWebState:(web::WebStateID)webStateID {
  // Do not show wait actions when they have zero duration.
  if (toolType == actor::ToolType::kWaitZeroDuration) {
    return;
  }
  [self queueSnackbarWithUpdate:[[SnackbarActorUpdate alloc]
                                    initWithTool:toolType
                                      taskUpdate:taskUpdate]];
}

- (void)actorTaskDidStopWithID:(actor::ActorTaskId)taskID
                    finalState:(actor::ActorTaskState)finalState {
  [self hideOverlay];
}

@end
