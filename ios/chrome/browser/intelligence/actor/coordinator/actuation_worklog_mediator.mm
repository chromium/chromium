// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_mediator.h"

#import <optional>

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

  // TODO(crbug.com/556191112): Add a catch-all or specific icons for the
  // remaining tool types.
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
    case actor::ToolType::kWait:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_WAITING)
                  icon:SymbolWithPointSize(SymbolHourglass, kIconSize)];
    case actor::ToolType::kAttemptLogin:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_FILLING_PASSWORD)
                  icon:SymbolWithPointSize(SymbolKey, kIconSize)];
    default:
      return nil;
  }
}

}  // namespace

@implementation ActuationWorklogMediator {
  // Latest emitted update, used to deduplicate consecutive identical updates.
  NSString* _latestEmittedTaskUpdate;
}

- (instancetype)init {
  self = [super init];
  return self;
}

- (void)disconnect {
  [_consumer reset];
  _consumer = nil;
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
  // TODO(crbug.com/555198195): Ensure updates are tied to the same `taskID`.
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
  [_consumer setActuationActive:!actor::IsTerminalState(newState)];
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
        willExecuteTool:(actor::ToolType)toolType
             taskUpdate:(NSString*)taskUpdate
             onWebState:(web::WebStateID)webStateID {
  [self processUpdateWithTool:toolType taskUpdate:taskUpdate];
}

- (void)actorTaskDidStopWithID:(actor::ActorTaskId)taskID
                    finalState:(actor::ActorTaskState)finalState {
  _latestEmittedTaskUpdate = nil;
  [_consumer setActuationActive:NO];
  [_consumer reset];
}

@end
