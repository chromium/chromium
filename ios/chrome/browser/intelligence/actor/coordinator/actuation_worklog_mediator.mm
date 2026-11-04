// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_mediator.h"

#import <optional>

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_consumer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_data_source.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_consumer.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Icon size for SF symbols in chips.
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
    case actor::ToolType::kBack:
    case actor::ToolType::kForward:
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
                           IDS_IOS_ACTOR_WORKLOG_CHIP_DEFAULT)
                  icon:SymbolWithPointSize(SymbolCursorArrow, kIconSize)];
  }
}

}  // namespace

@interface ActuationWorklogMediator () <ActorTaskDataSourceObserver>
@end

@implementation ActuationWorklogMediator {
  // Whether a data source is observed. Tracked separately because
  // `_dataSource` is weak.
  BOOL _observing;
  // Source of the worklog of the observed task.
  __weak id<ActorTaskDataSource> _dataSource;
  // Handler of the user actions on the worklog of the observed task.
  __weak id<ActorTaskActionHandler> _actionHandler;
  // Latest task update, displayed as the current step in the minimized header.
  NSString* _latestEmittedTaskUpdate;
  // Title of the observed task.
  NSString* _taskTitle;
  // Current state of the observed task.
  actor::ActorTaskState _taskState;
}

#pragma mark - Public

- (void)startObservingDataSource:(id<ActorTaskDataSource>)dataSource
                   actionHandler:(id<ActorTaskActionHandler>)actionHandler {
  if (_observing && _dataSource == dataSource) {
    return;
  }
  [self stopObservingTask];

  // Ensure we are brought back at the init state when subscribing to a task.
  _taskState = actor::ActorTaskState::kInit;
  _observing = YES;
  _actionHandler = actionHandler;
  _dataSource = dataSource;
  // Synchronously delivers the current worklog.
  [_dataSource addObserver:self];
}

- (void)stopObservingTask {
  if (!_observing) {
    return;
  }
  [_dataSource removeObserver:self];
  _dataSource = nil;
  _actionHandler = nil;
  _observing = NO;
  _latestEmittedTaskUpdate = nil;
  _taskTitle = nil;
  [self.consumer setActuationActive:NO];
  [self.consumer reset];
}

- (void)disconnect {
  [self stopObservingTask];
  self.consumer = nil;
  self.suggestionsConsumer = nil;
}

#pragma mark - Private

// Displays `item` with the chip of `toolType`. Task updates, displayed as
// simple items, become the current step.
- (void)displayItem:(ActuationWorklogItem*)item
           toolType:(std::optional<actor::ToolType>)toolType
           animated:(BOOL)animated {
  if (item.style == ActuationWorklogItemStyle::kSimple) {
    _latestEmittedTaskUpdate = item.title;
  }
  [self.consumer updateWorklogWithItem:item
                                  chip:ChipForToolType(toolType)
                              animated:animated];
}

// Updates the displayed title and state of the task.
- (void)applyTaskTitle:(NSString*)taskTitle
             taskState:(actor::ActorTaskState)taskState {
  _taskTitle = [taskTitle copy];
  _taskState = taskState;
  [self.consumer setActuationActive:!actor::IsTerminalState(taskState)];
  [self updateHeaderContent];
}

// Pushes the header content for the observed task to the consumer.
- (void)updateHeaderContent {
  ActuationHeaderContent* defaultContent =
      [[ActuationHeaderContent alloc] initWithTitle:_taskTitle subtitle:nil];
  [self.consumer setHeaderContentVariants:
                     [[ActuationHeaderContentVariants alloc]
                         initWithMinimizedContent:[self minimizedHeaderContent]
                                   compactContent:defaultContent
                                  expandedContent:defaultContent]];
}

// Returns the header content for the minimized display mode. While the task
// runs, the current step is shown below the task title. Otherwise, a status
// message replaces the title and the task title moves below it.
- (ActuationHeaderContent*)minimizedHeaderContent {
  switch (_taskState) {
    case actor::ActorTaskState::kInit:
    case actor::ActorTaskState::kActing:
    case actor::ActorTaskState::kReflecting:
    case actor::ActorTaskState::kPausedByActor:
    case actor::ActorTaskState::kPausedByUser:
      // TODO(crbug.com/571141885): Show a placeholder subtitle for the `kInit`
      // state, since at that point we don't have a task update yet.
      return [[ActuationHeaderContent alloc]
          initWithTitle:_taskTitle
               subtitle:_latestEmittedTaskUpdate];
    case actor::ActorTaskState::kWaitingOnUser:
      return [self contentWithStatus:IDS_IOS_ACTOR_HEADER_INPUT_NEEDED];
    case actor::ActorTaskState::kFinished:
      return [self contentWithStatus:IDS_IOS_ACTOR_HEADER_TASK_DONE];
    // `kCancelled` is meant to only represent user cancellations. Other stop
    // reasons are expected to map to `kFinished` or `kFailed` once
    // crbug.com/532978481 is fixed.
    case actor::ActorTaskState::kCancelled:
      return [self contentWithStatus:IDS_IOS_ACTOR_HEADER_TASK_STOPPED];
    case actor::ActorTaskState::kFailed:
      return [self contentWithStatus:IDS_IOS_ACTOR_HEADER_TASK_FAILED];
  }
}

// Returns header content with a status message above the task title.
- (ActuationHeaderContent*)contentWithStatus:(int)messageID {
  return [[ActuationHeaderContent alloc]
      initWithTitle:l10n_util::GetNSString(messageID)
           subtitle:_taskTitle];
}

#pragma mark - ActorTaskDataSourceObserver

- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
    didAttachWithTaskTitle:(NSString*)taskTitle
                 taskState:(actor::ActorTaskState)taskState
                     items:(NSArray<ActuationWorklogItem*>*)items
            latestToolType:(std::optional<actor::ToolType>)latestToolType {
  // The title is unknown until the task is registered with the data source.
  if (taskTitle) {
    [self applyTaskTitle:taskTitle taskState:taskState];
  }
  for (ActuationWorklogItem* item in items) {
    // Only the latest tool is displayed as a chip.
    std::optional<actor::ToolType> toolType =
        item == items.lastObject ? latestToolType : std::nullopt;
    [self displayItem:item toolType:toolType animated:NO];
  }
  if (items.count > 0) {
    [self updateHeaderContent];
  }
}

- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
    didUpdateTaskTitle:(NSString*)taskTitle
             taskState:(actor::ActorTaskState)taskState {
  [self applyTaskTitle:taskTitle taskState:taskState];
}

- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
         didAppendItem:(ActuationWorklogItem*)item
              toolType:(std::optional<actor::ToolType>)toolType {
  [self displayItem:item toolType:toolType animated:YES];
  [self updateHeaderContent];
}

- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
    didReplaceLastItem:(ActuationWorklogItem*)item {
  [self.consumer setLastItem:item chip:nil];
  // Only the suggestions card is replaced, after a reselection.
  [self.suggestionsConsumer
      updateActiveWorklogItemForReselectedSuggestion:item];
}

- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
    didUpdateIntervention:(ActuationInterventionData*)intervention {
  [self.consumer setIntervention:intervention];
}

- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
    didUpdateSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
                 forItem:(ActuationWorklogItem*)item {
  if (suggestions) {
    [self.suggestionsConsumer setSuggestions:suggestions
                        forActiveWorklogItem:item];
  } else {
    [self.suggestionsConsumer clearSuggestions];
  }
}

#pragma mark - ActuationWorklogMutator

- (void)stopActuation {
  [_actionHandler stopTask];
}

- (void)didTriggerInterventionAction:(ActuationInterventionAction*)action {
  [_actionHandler resolveInterventionWithAction:action];
}

#pragma mark - ActuationSuggestionsDelegate

- (void)suggestionsCoordinator:(ActuationSuggestionsCoordinator*)coordinator
           didSelectSuggestion:(ActorFormSuggestion*)suggestion {
  [_actionHandler selectSuggestion:suggestion];
}

@end
