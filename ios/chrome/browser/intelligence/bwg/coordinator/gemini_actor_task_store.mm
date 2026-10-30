// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_actor_task_store.h"

#import <optional>

#import "base/check.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/crb_protocol_observers_util.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Point size of the SF symbol of the first step.
constexpr CGFloat kIconSize = 16.0;

}  // namespace

@implementation GeminiActorTaskStore {
  // Null once disconnected or if the task couldn't be observed.
  raw_ptr<actor::ActorService> _actorService;
  // Observers of the worklog.
  CRBProtocolObservers<ActuationWorklogDataSourceObserver>* _observers;
  // Title of the task.
  NSString* _taskTitle;
  // Current state of the task.
  actor::ActorTaskState _taskState;
  // Items of the worklog, in display order.
  NSMutableArray<ActuationWorklogItem*>* _items;
  // Tool of the last item of `_items`, if any.
  std::optional<actor::ToolType> _latestToolType;
  // Latest appended task update, used to drop consecutive duplicates.
  NSString* _latestTaskUpdate;
}

- (instancetype)initWithTaskID:(actor::ActorTaskId)taskID
                  actorService:(actor::ActorService*)actorService {
  self = [super init];
  if (self) {
    _taskID = taskID;
    _taskState = actor::ActorTaskState::kInit;
    _observers =
        static_cast<CRBProtocolObservers<ActuationWorklogDataSourceObserver>*>(
            [CRBProtocolObservers
                observersWithProtocol:@protocol(
                                          ActuationWorklogDataSourceObserver)]);
    _items = [NSMutableArray array];
    if (actorService && actorService->AddTaskUpdatesObserver(taskID, self)) {
      _actorService = actorService;
    }
  }
  return self;
}

- (void)disconnect {
  if (_actorService) {
    _actorService->RemoveTaskUpdatesObserver(_taskID, self);
    _actorService = nullptr;
  }
}

#pragma mark - ActuationWorklogDataSource

- (void)addObserver:(id<ActuationWorklogDataSourceObserver>)observer {
  CHECK(observer);
  if (actor::ContainsObserver(_observers, observer)) {
    return;
  }
  [_observers addObserver:observer];
  [observer worklogDataSource:self
       didAttachWithTaskTitle:_taskTitle
                    taskState:_taskState
                        items:[_items copy]
               latestToolType:_latestToolType];
}

- (void)removeObserver:(id<ActuationWorklogDataSourceObserver>)observer {
  [_observers removeObserver:observer];
}

#pragma mark - ActorTaskUpdatesObserver

- (void)didRegisterAsObserverForTaskID:(actor::ActorTaskId)taskID
                             taskTitle:(NSString*)taskTitle
                            taskUpdate:(NSString*)taskUpdate
                          currentState:(actor::ActorTaskState)state
                             webStates:(NSArray<NSNumber*>*)webStatesIDs {
  _taskTitle = [taskTitle copy];
  _taskState = state;
  [self notifyTaskTitleOrStateUpdated];

  ActuationWorklogItem* firstItem = [ActuationWorklogItem
      labeledItemWithTitle:l10n_util::GetNSString(
                               IDS_IOS_GEMINI_FIRST_ACTUATION_STEP_TITLE)
                  subtitle:l10n_util::GetNSString(
                               IDS_IOS_GEMINI_FIRST_ACTUATION_STEP_SUBTITLE)
                      icon:SymbolWithPointSize(SymbolPlayFill, kIconSize)
                    active:YES];
  [self appendItem:firstItem toolType:std::nullopt];

  // The initial task update follows the Gemini first step.
  [self appendTaskUpdate:taskUpdate toolType:std::nullopt];
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
         didChangeState:(actor::ActorTaskState)newState
              fromState:(actor::ActorTaskState)oldState {
  _taskState = newState;
  [self notifyTaskTitleOrStateUpdated];
}

- (void)actorTaskWithID:(actor::ActorTaskId)taskID
        willExecuteTool:(actor::ToolType)toolType
             taskUpdate:(NSString*)taskUpdate
             onWebState:(web::WebStateID)webStateID {
  [self appendTaskUpdate:taskUpdate toolType:toolType];
}

#pragma mark - Private

// Appends an item for `taskUpdate` unless empty or a consecutive duplicate.
- (void)appendTaskUpdate:(NSString*)taskUpdate
                toolType:(std::optional<actor::ToolType>)toolType {
  // TODO(crbug.com/555839522): Support updating the tool chip even when the
  // `taskUpdate` is empty or deduplicated.
  if (taskUpdate.length == 0 ||
      [_latestTaskUpdate isEqualToString:taskUpdate]) {
    return;
  }
  _latestTaskUpdate = [taskUpdate copy];
  [self appendItem:[ActuationWorklogItem simpleItemWithTitle:taskUpdate
                                                      active:YES]
          toolType:toolType];
}

// Appends `item` to the worklog and notifies the observers.
- (void)appendItem:(ActuationWorklogItem*)item
          toolType:(std::optional<actor::ToolType>)toolType {
  [_items addObject:item];
  _latestToolType = toolType;
  [_observers worklogDataSource:self didAppendItem:item toolType:toolType];
}

// Notifies the observers of the current title and state of the task.
- (void)notifyTaskTitleOrStateUpdated {
  [_observers worklogDataSource:self
             didUpdateTaskTitle:_taskTitle
                      taskState:_taskState];
}

@end
