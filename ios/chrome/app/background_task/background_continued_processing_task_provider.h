// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_TASK_PROVIDER_H_
#define IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_TASK_PROVIDER_H_

#import <Foundation/Foundation.h>

@class BackgroundContinuedProcessingTaskRequest;

// Provides the continued processing tasks to request when background
// processing becomes available, and is told when it becomes unnecessary. The
// context of each started task is delivered through its request's
// `startedHandler`.
@protocol BackgroundContinuedProcessingTaskProvider <NSObject>

// Asks the provider for the processing tasks to request. Only called after
// `-backgroundProcessingBecameAvailable`. May return nil or an empty array.
- (NSArray<BackgroundContinuedProcessingTaskRequest*>*)
    continuedProcessingTaskRequests;

@optional

// Tells the provider that the app entered the background and background
// processing became available; its processing tasks are requested right after.
- (void)backgroundProcessingBecameAvailable;

// Tells the provider that the app returned to the foreground, so background
// processing is no longer necessary. Providers may still keep their tasks
// running.
- (void)backgroundProcessingBecameUnnecessary;

@end

#endif  // IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_TASK_PROVIDER_H_
