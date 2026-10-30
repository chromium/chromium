// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTUATION_WORKLOG_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTUATION_WORKLOG_DATA_SOURCE_H_

#import <Foundation/Foundation.h>

#import <optional>

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

@class ActuationWorklogItem;
@protocol ActuationWorklogDataSource;

// Observer of the worklog published by an `ActuationWorklogDataSource`.
// TODO(crbug.com/579385900): Replace the attach and incremental callbacks with
// a single immutable worklog state, pushed on attach and after every change.
@protocol ActuationWorklogDataSourceObserver <NSObject>

// Called synchronously when the observer is added, with the current worklog.
// `latestToolType` is the tool of the last item, if any.
- (void)worklogDataSource:(id<ActuationWorklogDataSource>)dataSource
    didAttachWithTaskTitle:(NSString*)taskTitle
                 taskState:(actor::ActorTaskState)taskState
                     items:(NSArray<ActuationWorklogItem*>*)items
            latestToolType:(std::optional<actor::ToolType>)latestToolType;

// Called when the title or the state of the task changes.
- (void)worklogDataSource:(id<ActuationWorklogDataSource>)dataSource
       didUpdateTaskTitle:(NSString*)taskTitle
                taskState:(actor::ActorTaskState)taskState;

// Called when `item` is appended, with the tool executed for it, if any.
- (void)worklogDataSource:(id<ActuationWorklogDataSource>)dataSource
            didAppendItem:(ActuationWorklogItem*)item
                 toolType:(std::optional<actor::ToolType>)toolType;

@end

// Source of the worklog of a single actor task.
@protocol ActuationWorklogDataSource <NSObject>

// Adds `observer` (held weakly) and sends it the current worklog. No-op if
// `observer` is already added.
- (void)addObserver:(id<ActuationWorklogDataSourceObserver>)observer;

// Removes `observer`.
- (void)removeObserver:(id<ActuationWorklogDataSourceObserver>)observer;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTUATION_WORKLOG_DATA_SOURCE_H_
