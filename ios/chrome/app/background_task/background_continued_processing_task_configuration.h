// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_TASK_CONFIGURATION_H_
#define IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_TASK_CONFIGURATION_H_

#import <BackgroundTasks/BackgroundTasks.h>
#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "base/time/time.h"
#import "ios/chrome/app/background_mode_buildflags.h"

// Default total units of progress for a continued processing task.
inline constexpr int64_t kDefaultTotalUnitsOfProgress = 1000;

// Default expected number of discrete steps during the linear progress phase of
// the stepped incremental progress.
inline constexpr int64_t kDefaultExpectedStepCount = 18;

// How the scheduler handles a continued processing request it cannot start
// immediately. Mapped to the BackgroundTasks SDK by the app agent.
enum class BackgroundContinuedProcessingSubmissionStrategy {
  // The system queues the request until it can run.
  kQueue,
  // Submission fails if the task cannot start now.
  kFail,
};

// Configuration object containing the parameters required to request a
// background continued processing task.
@interface BackgroundContinuedProcessingTaskConfiguration : NSObject

// Initial title displayed in the system-provided Live Activity.
@property(nonatomic, copy) NSString* title;

// Initial subtitle displayed in the system-provided Live Activity. Defaults to
// an empty string if set to nil (system requires non-null).
@property(nonatomic, copy) NSString* subtitle;

// Callback invoked when the system expires the task or the user cancels it from
// the Live Activity interface. Guaranteed to run on the main/UI thread.
@property(nonatomic, readonly, copy) ProceduralBlock expirationHandler;

// (Optional) Total units of work for progress tracking. Must be strictly
// positive. Defaults to `kDefaultTotalUnitsOfProgress`.
@property(nonatomic) int64_t totalUnits;

// (Optional) Expected number of discrete progress steps for the linear
// progress phase. Must be strictly positive. Defaults to
// `kDefaultExpectedStepCount`.
@property(nonatomic) int64_t expectedStepCount;

// (Optional) Interval at which progress advances by one unit so the task isn't
// considered stalled. Defaults to zero, which disables the heartbeat.
@property(nonatomic) base::TimeDelta progressHeartbeatInterval;

// (Optional) How the scheduler handles a request it cannot start immediately.
// Defaults to `kQueue`.
@property(nonatomic)
    BackgroundContinuedProcessingSubmissionStrategy submissionStrategy;

#if BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)
// (Optional) Special system resources required for the task. Defaults to
// `BGContinuedProcessingTaskRequestResourcesDefault`.
@property(nonatomic)
    BGContinuedProcessingTaskRequestResources requiredResources API_AVAILABLE(
        ios(26.0));
#endif  // BUILDFLAG(IOS_BACKGROUND_CONTINUED_PROCESSING_ENABLED)

// Initializes the configuration with the required parameters. `title` is
// mandatory and must not be empty. `subtitle` defaults to an empty string if
// nil. `expirationHandler` is guaranteed to run on the main/UI thread upon task
// expiration or cancellation.
- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
            expirationHandler:(ProceduralBlock)expirationHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_TASK_CONFIGURATION_H_
