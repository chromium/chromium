// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_H_

#import <Foundation/Foundation.h>

// Model representing a single level-up task item.
@interface LevelUpTask : NSObject

// Unique identifier for this task.
@property(nonatomic, copy, readonly) NSString* taskID;

// Title describing the task.
@property(nonatomic, copy, readonly) NSString* title;

// Description explaining the task features.
@property(nonatomic, copy, readonly) NSString* taskDescription;

// Name for this task's icon.
@property(nonatomic, copy, readonly) NSString* iconSymbolName;

// The user task completion state.
@property(nonatomic, assign, readonly) BOOL completed;

// The action executed when the task is selected to navigate to the relevant
// destination inside Chrome, enabling the user to perform the task.
@property(nonatomic, copy, readonly) void (^navigationAction)(void);

- (instancetype)initWithTaskID:(NSString*)taskID
                         title:(NSString*)title
               taskDescription:(NSString*)taskDescription
                iconSymbolName:(NSString*)iconSymbolName
                     completed:(BOOL)completed
              navigationAction:(void (^)(void))navigationAction
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_H_
