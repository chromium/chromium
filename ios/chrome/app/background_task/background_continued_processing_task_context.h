// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_TASK_CONTEXT_H_
#define IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_TASK_CONTEXT_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

@class BackgroundContinuedProcessingTaskConfiguration;
@class BGContinuedProcessingTask;

// Context representing an active continued processing background task.
// Used to interact with an existing task. This object and its properties are
// strictly sequence-affine and must only be accessed on the main thread.
@interface BackgroundContinuedProcessingTaskContext : NSObject

// The formatted system task identifier.
@property(nonatomic, readonly, copy) NSString* taskIdentifier;

// Title displayed in the system-provided Live Activity. Setting this property
// immediately updates the cached title and propagates the update to the system.
@property(nonatomic, copy) NSString* title;

// Subtitle displayed in the system-provided Live Activity. Setting this
// property immediately updates the cached subtitle and propagates the update
// to the system.
@property(nonatomic, copy) NSString* subtitle;

// The fraction of work completed (from 0.0 to 1.0).
@property(nonatomic, readonly) double fractionCompleted;

// Whether the task has already been completed or expired.
@property(nonatomic, readonly, getter=isCompleted) BOOL completed;

// Updates both the title and subtitle simultaneously in the system-provided
// Live Activity.
- (void)updateTitle:(NSString*)title subtitle:(NSString*)subtitle;

// Additively advances the completed progress units.
- (void)incrementProgressByUnits:(int64_t)units;

// Sets completed progress units directly.
- (void)setCompletedUnits:(int64_t)completedUnits;

// Signals task completion to the OS and manager.
- (void)setTaskCompletedWithSuccess:(BOOL)success;

#pragma mark - Internal

// Initializer used by `BackgroundContinuedProcessingAppAgent` to create the
// task context with an internal finish handler. Consumers should obtain
// instances via `[BackgroundContinuedProcessingAppAgent
// requestTaskWithIdentifier:configuration:]` rather than initializing directly.
- (instancetype)initWithTaskIdentifier:(NSString*)taskIdentifier
                         configuration:
                             (BackgroundContinuedProcessingTaskConfiguration*)
                                 configuration
                         finishHandler:(ProceduralBlock)finishHandler
    NS_DESIGNATED_INITIALIZER;

// Attaches an underlying system `BGContinuedProcessingTask` to the context once
// the OS launch handler fires, setting properties on it. Internal: consumers
// must not invoke this method.
- (void)attachUnderlyingTask:(BGContinuedProcessingTask*)task
    API_AVAILABLE(ios(26.0));

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_TASK_CONTEXT_H_
