// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_APP_AGENT_H_
#define IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_APP_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"

@class BackgroundContinuedProcessingTaskConfiguration;
@class BackgroundContinuedProcessingTaskContext;

// An app agent that manages background continued processing tasks.
// Coordinates scheduling with `BGTaskScheduler` and provides context handles
// to callers for tracking progress, system-provided Live Activity updates, and
// expiration handling. `requestTaskWithIdentifier:configuration:` must be
// called from an active foreground state.
@interface BackgroundContinuedProcessingAppAgent : ObservingAppAgent

// Requests a continued processing task with the given identifier and
// configuration. MUST be invoked while the application is in an active
// foreground (`UIApplicationStateActive`) state. Returns a handle immediately,
// or nil if background continued processing is disabled, running on iOS < 26,
// or submission fails (such as on simulator without runtime support).
// Requesting a task and receiving a handle does not guarantee the task will be
// executed right away, or at all. The app agent retains the context handle for
// the duration of the background operation until task completion or expiration.
// Callers are responsible for using the returned handle for driving progres and
// explicitly signaling task completion via `setTaskCompletedWithSuccess:` once
// work finishes to prevent the task from lingering until system expiration.
- (BackgroundContinuedProcessingTaskContext*)
    requestTaskWithIdentifier:(NSString*)identifier
                configuration:(BackgroundContinuedProcessingTaskConfiguration*)
                                  configuration;

@end

#endif  // IOS_CHROME_APP_BACKGROUND_TASK_BACKGROUND_CONTINUED_PROCESSING_APP_AGENT_H_
