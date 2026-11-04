// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_DATA_SOURCE_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

@class ActorFormSuggestion;
@class ActuationInterventionAction;
@class ActuationInterventionData;
@class ActuationWorklogItem;
@protocol ActorTaskDataSource;

// Observer of the worklog published by an `ActorTaskDataSource`.
// TODO(crbug.com/579385900): Replace the attach and incremental callbacks with
// a single immutable worklog state, pushed on attach and after every change.
@protocol ActorTaskDataSourceObserver <NSObject>

// Called synchronously when the observer is added, with the current worklog.
// `latestToolType` is the tool of the last item, if any.
- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
    didAttachWithTaskTitle:(NSString*)taskTitle
                 taskState:(actor::ActorTaskState)taskState
                     items:(NSArray<ActuationWorklogItem*>*)items
            latestToolType:(std::optional<actor::ToolType>)latestToolType;

// Called when the title or the state of the task changes.
- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
    didUpdateTaskTitle:(NSString*)taskTitle
             taskState:(actor::ActorTaskState)taskState;

// Called when `item` is appended, with the tool executed for it, if any.
- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
         didAppendItem:(ActuationWorklogItem*)item
              toolType:(std::optional<actor::ToolType>)toolType;

// Called when the last item is replaced by `item`, which has no tool chip, e.g.
// the suggestions card after a reselection.
- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
    didReplaceLastItem:(ActuationWorklogItem*)item;

// Called when the pending intervention changes (`nil` once resolved or
// discarded), and after attach if one is pending.
- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
    didUpdateIntervention:(ActuationInterventionData*)intervention;

// Called when `suggestions` become selectable for `item` (`nil` once the user
// resolves the intervention), and after attach if some are selectable.
- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
    didUpdateSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
                 forItem:(ActuationWorklogItem*)item;

@end

// Source of the worklog of a single actor task. Implemented by features (e.g.
// `GeminiActorTaskStore`), not by `ActorService`.
@protocol ActorTaskDataSource <NSObject>

// Adds `observer` (held weakly) and sends it the current worklog. No-op if
// `observer` is already added.
- (void)addObserver:(id<ActorTaskDataSourceObserver>)observer;

// Removes `observer`.
- (void)removeObserver:(id<ActorTaskDataSourceObserver>)observer;

@end

// Handles the user actions on the worklog of a single actor task.
@protocol ActorTaskActionHandler <NSObject>

// Resolves the pending intervention with `action`. No-op if none is pending.
- (void)resolveInterventionWithAction:(ActuationInterventionAction*)action;

// Selects `suggestion` for the pending suggestions intervention, replacing the
// card of the previously selected suggestion. No-op if no suggestions are
// selectable.
- (void)selectSuggestion:(ActorFormSuggestion*)suggestion;

// Stops the task at the user's request.
- (void)stopTask;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_DATA_SOURCE_H_
