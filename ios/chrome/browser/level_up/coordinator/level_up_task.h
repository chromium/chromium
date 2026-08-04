// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_TASK_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_TASK_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/level_up/model/task_types.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

class TaskInfo;

// Model representing a single level-up task item, wrapping a C++ TaskInfo.
@interface LevelUpTask : NSObject

// Unique identifier for this task.
@property(nonatomic, copy, readonly) NSString* taskID;

// Title describing the task.
@property(nonatomic, copy, readonly) NSString* title;

// Description explaining the task features.
@property(nonatomic, copy, readonly) NSString* taskDescription;

// Icon symbol for this task.
@property(nonatomic, assign, readonly) Symbol iconSymbol;

// The user task completion state.
@property(nonatomic, assign, readonly) BOOL completed;

// The category grouping this task belongs to.
@property(nonatomic, assign, readonly) LevelUpTaskCategory category;

// The backing C++ task info model.
@property(nonatomic, assign, readonly) const TaskInfo* taskInfo;

- (instancetype)initWithTaskInfo:(const TaskInfo*)taskInfo
                       completed:(BOOL)completed NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_TASK_H_
