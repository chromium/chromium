// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_TASK_CONFIGURATION_H_
#define IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_TASK_CONFIGURATION_H_

#import <BackgroundTasks/BackgroundTasks.h>
#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

// Configuration object containing the parameters required to request a
// background continued processing task.
@interface BackgroundContinuedProcessingTaskConfiguration : NSObject

// Initial title displayed in the system-provided Live Activity.
@property(nonatomic, copy) NSString* title;

// (Optional) Initial subtitle displayed in the system-provided Live Activity.
@property(nonatomic, copy) NSString* subtitle;

// Callback invoked when the system expires the task or the user cancels it from
// the Live Activity interface. Guaranteed to run on the main/UI thread.
@property(nonatomic, readonly, copy) ProceduralBlock expirationHandler;

// (Optional) Total units of work for progress tracking. Defaults to 100.
@property(nonatomic) int64_t totalUnits;

// (Optional) The submission strategy for the scheduler to abide by. Defaults to
// `BGContinuedProcessingTaskRequestSubmissionStrategyQueue`.
@property(nonatomic)
    BGContinuedProcessingTaskRequestSubmissionStrategy strategy API_AVAILABLE(
        ios(26.0));

// (Optional) Special system resources required for the task. Defaults to
// `BGContinuedProcessingTaskRequestResourcesDefault`.
@property(nonatomic)
    BGContinuedProcessingTaskRequestResources requiredResources API_AVAILABLE(
        ios(26.0));

// Initializes the configuration with the required parameters. `title` must not
// be empty. `expirationHandler` is guaranteed to run on the main/UI thread upon
// task expiration or cancellation.
- (instancetype)initWithTitle:(NSString*)title
            expirationHandler:(ProceduralBlock)expirationHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_TASK_CONFIGURATION_H_
