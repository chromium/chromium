// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_mediator.h"

#import <optional>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_consumer.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Icon size for SF symbols in chips and labeled items.
constexpr CGFloat kIconSize = 16.0;

// Generates an `ActuationWorklogChip` based on the executed tool type. Returns
// `nil` for tool types that don't display a chip.
ActuationWorklogChip* ChipForToolType(std::optional<actor::ToolType> toolType) {
  if (!toolType) {
    return nil;
  }
  switch (*toolType) {
    case actor::ToolType::kClick:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_CLICKING)
                  icon:SymbolWithPointSize(SymbolCursorArrowRays, kIconSize)];
    case actor::ToolType::kType:
    case actor::ToolType::kAttemptFormFilling:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_CHIP_TYPING)
                  icon:SymbolWithPointSize(SymbolKeyboard, kIconSize)];
    case actor::ToolType::kScroll:
    case actor::ToolType::kScrollTo:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_SCROLLING)
                  icon:SymbolWithPointSize(SymbolCursorArrowMotionLines,
                                           kIconSize)];
    case actor::ToolType::kAttemptLogin:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_FILLING_PASSWORD)
                  icon:SymbolWithPointSize(SymbolKey, kIconSize)];
    case actor::ToolType::kNavigate:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_NAVIGATING)
                  icon:SymbolWithPointSize(SymbolGlobe, kIconSize)];
    case actor::ToolType::kWait:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_WAITING)
                  icon:SymbolWithPointSize(SymbolHourglass, kIconSize)];
    case actor::ToolType::kWaitZeroDuration:
    case actor::ToolType::kUnknown:
      return nil;
    default:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_PROCESSING)
                  icon:SymbolWithPointSize(SymbolCursorArrow, kIconSize)];
  }
}

}  // namespace

@implementation ActuationWorklogMediator {
  // Service tracking actor tasks and updates.
  raw_ptr<actor::ActorService> _actorService;
  // Currently observed task ID.
  std::optional<actor::ActorTaskId> _currentTaskId;
  // Latest emitted update, used to deduplicate consecutive identical updates.
  NSString* _latestEmittedTaskUpdate;
}

- (instancetype)initWithActorService:(actor::ActorService*)actorService {
  self = [super init];
  if (self) {
    _actorService = actorService;
  }
  return self;
}

- (void)connect {
  if (_actorService) {
    _actorService->AddTaskUpdatesObserver(self);
  }
}

- (void)disconnect {
  if (_actorService) {
    _actorService->RemoveTaskUpdatesObserver(self);
    _actorService = nullptr;
  }
  [_consumer reset];
  _consumer = nil;
  _currentTaskId.reset();
  _latestEmittedTaskUpdate = nil;
}

#pragma mark - Private

// Emits a worklog item and tool chip, deduplicating consecutive updates.
- (void)processUpdateWithTool:(std::optional<actor::ToolType>)toolType
                   taskUpdate:(NSString*)taskUpdate {
  // TODO(crbug.com/555839522): Support updating the tool chip even when the
  // `taskUpdate` is nil.
  if (!_consumer || taskUpdate.length == 0) {
    return;
  }

  // Deduplicate consecutive identical updates.
  // TODO(crbug.com/555839522): Support updating the tool chip even when the
  // `taskUpdate` is deduplicated.
  if ([_latestEmittedTaskUpdate isEqualToString:taskUpdate]) {
    return;
  }
  _latestEmittedTaskUpdate = [taskUpdate copy];

  ActuationWorklogItem* item =
      [ActuationWorklogItem simpleItemWithTitle:taskUpdate active:YES];
  ActuationWorklogChip* chip = ChipForToolType(toolType);
  [_consumer updateWorklogWithItem:item chip:chip animated:YES];
}

#pragma mark - ActorTaskUpdatesObserver

- (void)didRegisterAsObserverForTaskID:(actor::ActorTaskId)taskID
                             taskTitle:(NSString*)taskTitle
                            taskUpdate:(NSString*)taskUpdate
                          currentState:(actor::ActorTaskState)state
                             webStates:(NSArray<NSNumber*>*)webStatesIDs {
  _currentTaskId = taskID;
  ActuationWorklogItem* initialItem = [ActuationWorklogItem
      labeledItemWithTitle:l10n_util::GetNSString(
                               IDS_IOS_GEMINI_FIRST_ACTUATION_STEP_TITLE)
                  subtitle:l10n_util::GetNSString(
                               IDS_IOS_GEMINI_FIRST_ACTUATION_STEP_SUBTITLE)
                      icon:SymbolWithPointSize(SymbolPlayFill, kIconSize)
                    active:YES];
  [_consumer setTaskTitle:taskTitle];
  [_consumer setActuationActive:!actor::IsTerminalState(state)];
  [_consumer updateWorklogWithItem:initialItem chip:nil animated:YES];

  // Use the initial task update on top of our default start task update.
  [self processUpdateWithTool:std::nullopt taskUpdate:taskUpdate];
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
         didChangeState:(actor::ActorTaskState)newState
              fromState:(actor::ActorTaskState)oldState {
  if (_currentTaskId != taskID) {
    return;
  }
  [_consumer setActuationActive:!actor::IsTerminalState(newState)];
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
        willExecuteTool:(actor::ToolType)toolType
             taskUpdate:(NSString*)taskUpdate
             onWebState:(web::WebStateID)webStateID {
  if (_currentTaskId != taskID) {
    return;
  }
  [self processUpdateWithTool:toolType taskUpdate:taskUpdate];
}

- (void)actorTaskDidStopWithID:(actor::ActorTaskId)taskID
                    finalState:(actor::ActorTaskState)finalState {
  if (_currentTaskId != taskID) {
    return;
  }
  _currentTaskId.reset();
  _latestEmittedTaskUpdate = nil;
  [_consumer setActuationActive:NO];
  [_consumer reset];
}

#pragma mark - ActuationWorklogMutator

- (void)stopActuation {
  if (!_actorService || !_currentTaskId) {
    return;
  }
  _actorService->StopTask(*_currentTaskId,
                          actor::ActorTaskStoppedReason::kStoppedByUser);
}

@end
