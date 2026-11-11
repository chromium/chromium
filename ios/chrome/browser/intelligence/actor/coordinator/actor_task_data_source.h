// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTOR_TASK_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTOR_TASK_DATA_SOURCE_H_

#import <Foundation/Foundation.h>

@class ActorFormSuggestion;
@class ActuationInterventionAction;
@class ActuationWorklogState;
@protocol ActorTaskDataSource;

// Observer of the worklog published by an `ActorTaskDataSource`.
@protocol ActorTaskDataSourceObserver <NSObject>

// Called synchronously when the observer is added, and after every change.
// `state` is `nil` until the task is registered.
- (void)taskDataSource:(id<ActorTaskDataSource>)dataSource
        didUpdateState:(ActuationWorklogState*)state;

@end

// Source of the worklog of a single actor task. Implemented by features (e.g.
// `GeminiActorTaskStore`), not by `ActorService`.
@protocol ActorTaskDataSource <NSObject>

// Current worklog, `nil` until the task is registered.
// TODO(crbug.com/579385900): Make it non-nil once tasks are only handed to the
// UI after their registration.
@property(nonatomic, readonly) ActuationWorklogState* state;

// Adds `observer` (held weakly) and sends it `state`. No-op if `observer` is
// already added.
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

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTOR_TASK_DATA_SOURCE_H_
