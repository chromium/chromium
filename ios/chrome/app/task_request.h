// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TASK_REQUEST_H_
#define IOS_CHROME_APP_TASK_REQUEST_H_

#import <Foundation/Foundation.h>

#import <string_view>

@class UIOpenURLContext;
@class NSUserActivity;
@class UIApplicationShortcutItem;
@class SceneState;

// Defines the point at which a task can be executed. These stages should be
// specific to a scene.
enum class TaskExecutionStage {
  TaskExecutionStageNone = 0,
  // Profile has been attached to a given scene.
  TaskExecutionProfileLoaded,
  // UI is ready for a scene.
  TaskExecutionUIReady,
};

// Define the signature for the shortcut completion handler.
using ShortcutCompletionHandler = void (^)(BOOL succeeded);

// Represents a task to be executed by the application, typically triggered by
// external entry points such as opening a URL, responding to a
// NSUserActivity, or selecting a home screen shortcut. TaskRequest
// encapsulates the necessary context for the task, including its source, the
// target scene, and the minimum application lifecycle stage required before
// it can be safely executed.
@interface TaskRequest : NSObject

// Gaia ID associated with the task, if any.
@property(nonatomic, readonly) NSString* gaiaID;

// Minimum execution stage for a task.
@property(nonatomic, assign) TaskExecutionStage minimumStage;

// Scene session ID on which the task should be executed.
@property(nonatomic, readonly) std::string_view sceneSessionID;

// True if the task was created during a cold start.
@property(nonatomic, readonly) BOOL isColdStart;

// Factory methods for the different task types.
+ (instancetype)taskForURLContext:(UIOpenURLContext*)URLContext
                       sceneState:(SceneState*)sceneState
                      isColdStart:(BOOL)isColdStart;

+ (instancetype)taskForUserActivity:(NSUserActivity*)userActivity
                         sceneState:(SceneState*)sceneState
                        isColdStart:(BOOL)isColdStart;

+ (instancetype)taskForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                         sceneState:(SceneState*)sceneState
                            handler:(ShortcutCompletionHandler)handler
                        isColdStart:(BOOL)isColdStart;

// Executes the task.
- (void)execute;

@end

#endif  // IOS_CHROME_APP_TASK_REQUEST_H_
