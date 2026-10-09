// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_TASK_REQUEST_H_
#define IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_TASK_REQUEST_H_

#import <Foundation/Foundation.h>

@class BackgroundContinuedProcessingTaskConfiguration;
@class BackgroundContinuedProcessingTaskContext;

// A continued processing task that a provider wants the app agent to request.
@interface BackgroundContinuedProcessingTaskRequest : NSObject

// Client identifier of the task, also used by providers for correlation.
@property(nonatomic, readonly, copy) NSString* identifier;

// Configuration of the task.
@property(nonatomic, readonly, strong)
    BackgroundContinuedProcessingTaskConfiguration* configuration;

// Called with the started task's context once the request is submitted
// successfully. May be nil.
@property(nonatomic, readonly, copy) void (^startedHandler)
    (BackgroundContinuedProcessingTaskContext* context);

// `identifier` must not be empty and `configuration` must not be nil.
- (instancetype)
    initWithIdentifier:(NSString*)identifier
         configuration:
             (BackgroundContinuedProcessingTaskConfiguration*)configuration
        startedHandler:
            (void (^)(BackgroundContinuedProcessingTaskContext* context))
                startedHandler NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_TASK_REQUEST_H_
