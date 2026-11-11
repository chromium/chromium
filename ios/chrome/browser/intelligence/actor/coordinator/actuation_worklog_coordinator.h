// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ActuationWorklogViewController;
@protocol ActorTaskActionHandler;
@protocol ActorTaskDataSource;

// Coordinator managing the actuation worklog lifecycle. Displays the worklog
// published by an `ActorTaskDataSource` and forwards the user actions
// to an `ActorTaskActionHandler`.
@interface ActuationWorklogCoordinator : ChromeCoordinator

// The view controller displaying the actuation worklog.
@property(nonatomic, readonly) ActuationWorklogViewController* viewController;

// Starts displaying the worklog published by `dataSource`, replacing any
// previously observed one. The user actions on the worklog are forwarded to
// `actionHandler`. Must be called after `start`.
- (void)startObservingDataSource:(id<ActorTaskDataSource>)dataSource
                   actionHandler:(id<ActorTaskActionHandler>)actionHandler;

// Stops observing the current data source and resets the worklog. No-op if
// none is observed.
- (void)stopObservingDataSource;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_COORDINATOR_H_
